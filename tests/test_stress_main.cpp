#include <cpptest.h>

#include <memory>

std::unique_ptr<Test::Suite> make_pipeline_stress_suite();

int main() {
  Test::Suite root;
  root.add(make_pipeline_stress_suite());

  Test::TextOutput output(Test::TextOutput::Verbose);
  const bool ok = root.run(output);
  return ok ? 0 : 1;
}
