#include <argtree/argtree.hpp>

#include <iostream>
#include <string_view>

namespace {

struct CliState {
  bool version_requested{false};
};

void show_version(CliState const&) noexcept {
  std::cout << "sylogctl 0.2\n";
}

using Cli = argtree::cx::schema<CliState>;

const auto app = Cli::root(
    Cli::terminal_doc("version", "Print sylogctl version", &show_version));

} // namespace

int main(int argc, char** argv) {
  if (argc <= 1) {
    auto parsed = app.parse(0, static_cast<char**>(nullptr));
    app.print_help(std::cout, parsed, argc > 0 ? std::string_view{argv[0]} : std::string_view{"sylogctl"});
    return 0;
  }

  return app.run(argc, argv);
}
