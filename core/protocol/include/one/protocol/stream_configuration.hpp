#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace one::protocol {

inline constexpr std::string_view moonlight_common_revision =
    "874ac9548f1bd6f095ef2b435c42cdde460e7821";

enum class StreamLocation {
  automatic,
  local,
  remote,
};

enum class AudioLayout {
  stereo,
  surround_5_1,
  surround_7_1,
};

enum class ColorSpace {
  rec_601,
  rec_709,
  rec_2020,
};

enum class ColorRange {
  limited,
  full,
};

struct VideoFormats {
  bool h264 {true};
  bool hevc {true};
  bool hevc_10_bit {true};
  bool av1 {true};
  bool av1_10_bit {true};
};

struct StreamSettings {
  int width {1920};
  int height {1080};
  int frames_per_second {60};
  int bitrate_kbps {20'000};
  int packet_size {1024};
  StreamLocation location {StreamLocation::automatic};
  AudioLayout audio_layout {AudioLayout::stereo};
  VideoFormats video_formats {};
  int client_refresh_rate_x100 {6000};
  ColorSpace color_space {ColorSpace::rec_709};
  ColorRange color_range {ColorRange::limited};
  bool encrypt_audio {true};
  bool encrypt_video {true};
};

class StreamConfiguration final {
 public:
  explicit StreamConfiguration(StreamSettings settings = {});
  ~StreamConfiguration();

  StreamConfiguration(StreamConfiguration&&) noexcept;
  StreamConfiguration& operator=(StreamConfiguration&&) noexcept;

  StreamConfiguration(const StreamConfiguration&) = delete;
  StreamConfiguration& operator=(const StreamConfiguration&) = delete;

  [[nodiscard]] const StreamSettings& settings() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string launch_url_query_parameters();

}  // namespace one::protocol
