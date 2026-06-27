#include <cpptest.h>

#include <memory>

// Factories provided by each translation unit.
std::unique_ptr<Test::Suite> make_async_suite();
std::unique_ptr<Test::Suite> make_filters_suite();
std::unique_ptr<Test::Suite> make_renderers_suite();
std::unique_ptr<Test::Suite> make_queue_strategies_suite();
std::unique_ptr<Test::Suite> make_crossbar_sink_suite();
std::unique_ptr<Test::Suite> make_file_rotation_suite();
std::unique_ptr<Test::Suite> make_pipeline_lifecycle_suite();

int main()
{
  Test::Suite root;

  root.add(make_async_suite());
  root.add(make_filters_suite());
  root.add(make_renderers_suite());
  root.add(make_queue_strategies_suite());
  root.add(make_crossbar_sink_suite());
  root.add(make_file_rotation_suite());
  root.add(make_pipeline_lifecycle_suite());

  Test::TextOutput output(Test::TextOutput::Verbose);
  const bool ok = root.run(output);
  return ok ? 0 : 1;
}
