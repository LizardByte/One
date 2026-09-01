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
  const auto default_launch = defaults.launch_parameters();
  failures += expect(default_launch.width == 1920 && default_launch.height == 1080, "launch dimensions should match the stream");
  failures += expect(default_launch.remote_input_aes_key_hex.size() == 32, "remote input key should contain 16 bytes");
  failures += expect(default_launch.surround_audio_info != 0, "launch parameters should contain the audio layout");
  failures += expect(default_launch.hdr_requested, "10-bit formats should request HDR capability negotiation");
  failures += expect(
      default_launch.moonlight_query_parameters.find("corever=") != std::string::npos,
      "launch parameters should include moonlight-common-c extensions");

  one::protocol::StreamConfiguration second_defaults;
  failures += expect(
      second_defaults.launch_parameters().remote_input_aes_key_hex != default_launch.remote_input_aes_key_hex,
      "each stream should use a fresh remote input key");

  auto custom_settings = one::protocol::StreamSettings {};
  custom_settings.width = 3840;
  custom_settings.height = 2160;
  custom_settings.frames_per_second = 120;
  custom_settings.location = one::protocol::StreamLocation::remote;
  custom_settings.audio_layout = one::protocol::AudioLayout::surround_7_1;
  one::protocol::StreamConfiguration custom(std::move(custom_settings));
  failures += expect(custom.settings().width == 3840, "custom stream width should be retained");
  failures += expect(custom.settings().frames_per_second == 120, "custom frame rate should be retained");
  failures += expect(
      custom.launch_parameters().surround_audio_info != default_launch.surround_audio_info,
      "custom audio layout should be reflected in launch parameters");

  try {
    auto invalid_settings = one::protocol::StreamSettings {};
    invalid_settings.video_formats = {.h264 = false, .hevc = false, .hevc_10_bit = false, .av1 = false, .av1_10_bit = false};
    one::protocol::StreamConfiguration invalid(std::move(invalid_settings));
    failures += expect(false, "configuration with no codecs should be rejected");
  } catch (const std::invalid_argument&) {
  }

  return failures == 0 ? 0 : 1;
}
