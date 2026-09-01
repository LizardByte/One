#include <iostream>
#include <string_view>

#include "one/ui_mode.hpp"

namespace {

int expect_mode(const std::string_view name, const one::DeviceTraits& traits, const one::UiMode expected) {
  const auto actual = one::select_ui_mode(traits);
  if (actual == expected) {
    return 0;
  }

  std::cerr << name << ": expected " << one::to_string(expected) << ", got " << one::to_string(actual) << '\n';
  return 1;
}

}  // namespace

int main() {
  int failures = 0;
  failures += expect_mode(
      "desktop pointer",
      {one::DeviceClass::desktop, true, false, true},
      one::UiMode::desktop);
  failures += expect_mode(
      "phone touch",
      {one::DeviceClass::handheld, false, true, false},
      one::UiMode::touch);
  failures += expect_mode(
      "tablet with pointer",
      {one::DeviceClass::tablet, true, true, false},
      one::UiMode::touch);
  failures += expect_mode(
      "declared television",
      {one::DeviceClass::television, false, false, true},
      one::UiMode::television);
  failures += expect_mode(
      "directional-only device",
      {one::DeviceClass::unknown, false, false, true},
      one::UiMode::television);
  failures += expect_mode(
      "desktop with controller",
      {one::DeviceClass::desktop, true, false, true},
      one::UiMode::desktop);
  failures += expect_mode(
      "unknown fallback",
      {},
      one::UiMode::desktop);

  return failures == 0 ? 0 : 1;
}

