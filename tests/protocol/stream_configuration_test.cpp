#include <iostream>
#include <stdexcept>
#include <utility>

#include "one/protocol/stream_configuration.hpp"

namespace {

int expect(const bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << message << '\n';
  return 1;
}

}  // namespace

int main() {
  int failures = 0;

  one::protocol::StreamConfiguration defaults;
  failures += expect(defaults.settings().width == 1920, "default stream width should be 1920");
  failures += expect(defaults.settings().location == one::protocol::StreamLocation::automatic, "location should default to auto");
  failures += expect(
      one::protocol::moonlight_common_revision == "874ac9548f1bd6f095ef2b435c42cdde460e7821",
      "protocol revision should match the vendored source");
  failures += expect(!one::protocol::launch_url_query_parameters().empty(), "Sunshine launch parameters should be available");

  auto custom_settings = one::protocol::StreamSettings {};
  custom_settings.width = 3840;
  custom_settings.height = 2160;
  custom_settings.frames_per_second = 120;
  custom_settings.location = one::protocol::StreamLocation::remote;
  custom_settings.audio_layout = one::protocol::AudioLayout::surround_7_1;
  one::protocol::StreamConfiguration custom(std::move(custom_settings));
  failures += expect(custom.settings().width == 3840, "custom stream width should be retained");
  failures += expect(custom.settings().frames_per_second == 120, "custom frame rate should be retained");

  try {
    auto invalid_settings = one::protocol::StreamSettings {};
    invalid_settings.video_formats = {.h264 = false, .hevc = false, .hevc_10_bit = false, .av1 = false, .av1_10_bit = false};
    one::protocol::StreamConfiguration invalid(std::move(invalid_settings));
    failures += expect(false, "configuration with no codecs should be rejected");
  } catch (const std::invalid_argument&) {
  }

  return failures == 0 ? 0 : 1;
}
