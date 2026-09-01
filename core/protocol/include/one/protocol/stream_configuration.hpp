#pragma once

#include <cstdint>
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

struct StreamLaunchParameters {
  int width {0};
  int height {0};
  int frames_per_second {0};
  int surround_audio_info {0};
  std::string remote_input_aes_key_hex;
  std::uint32_t remote_input_aes_key_id {0};
  bool hdr_requested {false};
  std::string moonlight_query_parameters;
};

class StreamingSession;

class StreamConfiguration final {
 public:
  explicit StreamConfiguration(StreamSettings settings = {});
  ~StreamConfiguration();

  StreamConfiguration(StreamConfiguration&&) noexcept;
  StreamConfiguration& operator=(StreamConfiguration&&) noexcept;

  StreamConfiguration(const StreamConfiguration&) = delete;
  StreamConfiguration& operator=(const StreamConfiguration&) = delete;

  [[nodiscard]] const StreamSettings& settings() const noexcept;
  [[nodiscard]] StreamLaunchParameters launch_parameters() const;

 private:
  friend class StreamingSession;

  [[nodiscard]] void* native_configuration() noexcept;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string launch_url_query_parameters();

}  // namespace one::protocol
