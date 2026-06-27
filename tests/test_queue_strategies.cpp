#include <cpptest.h>

#include "sylog/sylog.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <future>

#include "test_helpers.hpp"
using namespace std::chrono_literals;

namespace {

    struct GateSink final : sylog::ISink {
        std::mutex m;
        std::condition_variable cv;
        bool entered = false;
        bool released = false;

        std::vector<std::string> lines;

        void write(std::string_view bytes) override {
            { // signal we've entered write()
                std::lock_guard lk(m);
                entered = true;
                cv.notify_all();
            }

            { // wait until test releases us
                std::unique_lock lk(m);
                cv.wait(lk, [&] { return released; });
            }

            lines.emplace_back(bytes);
        }

        bool wait_entered_for(std::chrono::milliseconds d) {
            std::unique_lock lk(m);
            return cv.wait_for(lk, d, [&] { return entered; });
        }

        void release() {
            std::lock_guard lk(m);
            released = true;
            cv.notify_all();
        }
        void flush() override {}
    };

    class QueueStrategiesSuite : public Test::Suite {
    public:
        QueueStrategiesSuite() {
            TEST_ADD(QueueStrategiesSuite::queue_strategy_block_blocks_when_full);
        }

        void queue_strategy_block_blocks_when_full() {
            using namespace sylog;
            using namespace std::chrono_literals;

            Pipeline::instance().close();
            Pipeline::instance().clear_routes();
            Pipeline::instance().clear_global_filters();
            Pipeline::instance().configure(Config::asynchronous(2));

            auto sink = std::make_shared<GateSink>();

            Route r;
            r.send_to(sink);
            r.render_with(std::make_shared<PatternRenderer>("{message}\n"));
            r.block_when_full();
            Pipeline::instance().add_route(r);

            Logger log("x");

            // IMPORTANT: do NOT start pipeline yet - no consumer.
            // Fill the queue to capacity.
            log.info("m1");
            log.info("m2");

            std::promise<void> producer_started;
            auto producer_started_fut = producer_started.get_future();

            std::promise<void> producer_finished;
            auto producer_finished_fut = producer_finished.get_future();
            std::atomic<int> state = 0;

            std::thread producer([&] {
                producer_started.set_value();
                state = 1;
                log.info("m3");                 // should block until consumer runs
                state = 2;
                producer_finished.set_value();
                });

            TEST_ASSERT_MSG(producer_started_fut.wait_for(2s) == std::future_status::ready,
                "producer thread did not start in time");

            // Now the thread has reached log.info("m3").
            // With capacity=2 already full and no consumer, it MUST still be blocked.

            TEST_ASSERT_MSG(producer_finished_fut.wait_for(10ms) == std::future_status::timeout,
                "expected enqueue to block when queue is full");

            // The producer must have entered the second log call but not completed yet.
            TEST_ASSERT_MSG(state.load() >= 1, "producer did not reach the enqueue call");
            TEST_ASSERT_MSG(state.load() == 1, "producer unexpectedly finished early");

            // Start pipeline so it drains one event and unblocks m3
            Pipeline::instance().start();

            TEST_ASSERT_MSG(producer_finished_fut.wait_for(2s) == std::future_status::ready,
                "producer did not finish after starting the pipeline");

            TEST_ASSERT_MSG(state.load() >= 2, "producer did not unblock after starting the pipeline");

            producer.join();

            sink->release();
            Pipeline::instance().close();

            // Optional: now you can assert delivery if you want (not needed for the blocking contract).
            // TEST_ASSERT_MSG(sink->lines.size() >= 1, "expected at least one line");
        }
    };
} // namespace

std::unique_ptr<Test::Suite> make_queue_strategies_suite() {
  return std::make_unique<QueueStrategiesSuite>();
}
