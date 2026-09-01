#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "one/protocol/stream_configuration.hpp"

namespace one::protocol {

struct ServerInformation {
  std::string address;
  std::string app_version;
  std::string gfe_version;
  std::string rtsp_session_url;
  int codec_mode_support {1};
};

enum class VideoBufferType {
  picture,
  sequence_parameter_set,
  picture_parameter_set,
  video_parameter_set,
  unknown,
};

enum class VideoFormat {
  h264,
  h264_444,
  hevc,
  hevc_10_bit,
  hevc_444,
  hevc_10_bit_444,
  av1,
  av1_10_bit,
  av1_444,
  av1_10_bit_444,
  unknown,
};

struct EncodedVideoBufferView {
  std::span<const std::byte> bytes;
  VideoBufferType type {VideoBufferType::unknown};
};

class EncodedVideoFrameView final {
 public:
  [[nodiscard]] int frame_number() const noexcept;
  [[nodiscard]] bool key_frame() const noexcept;
  [[nodiscard]] std::uint16_t host_processing_latency() const noexcept;
  [[nodiscard]] std::uint64_t receive_time_us() const noexcept;
  [[nodiscard]] std::uint64_t enqueue_time_us() const noexcept;
  [[nodiscard]] std::uint64_t presentation_time_us() const noexcept;
  [[nodiscard]] std::uint32_t rtp_timestamp() const noexcept;
  [[nodiscard]] std::size_t data_size() const noexcept;
  [[nodiscard]] bool hdr_active() const noexcept;
  [[nodiscard]] ColorSpace color_space() const noexcept;
  [[nodiscard]] std::size_t buffer_count() const noexcept;
  [[nodiscard]] EncodedVideoBufferView buffer(std::size_t index) const noexcept;

 private:
  friend class StreamingSession;
  explicit EncodedVideoFrameView(const void* native) noexcept;

  const void* native_ {nullptr};
};

struct VideoStreamInformation {
  VideoFormat format {VideoFormat::unknown};
  int width {0};
  int height {0};
  int redraw_rate {0};
};

struct VideoRendererCapabilities {
  bool direct_submit {false};
  bool reference_frame_invalidation_h264 {false};
  bool reference_frame_invalidation_hevc {false};
  bool reference_frame_invalidation_av1 {false};
  std::uint8_t slices_per_frame {0};
};

enum class VideoSubmitResult : int {
  ok = 0,
  request_key_frame = -1,
};

class VideoRenderer {
 public:
  virtual ~VideoRenderer() = default;
  [[nodiscard]] virtual VideoRendererCapabilities capabilities() const noexcept { return {}; }
  virtual int setup(const VideoStreamInformation& stream) noexcept = 0;
  virtual void start() noexcept = 0;
  virtual void stop() noexcept = 0;
  virtual void cleanup() noexcept = 0;
  virtual VideoSubmitResult submit(const EncodedVideoFrameView& frame) noexcept = 0;
};

struct OpusStreamInformation {
  AudioLayout layout {AudioLayout::stereo};
  int sample_rate {0};
  int channel_count {0};
  int streams {0};
  int coupled_streams {0};
  int samples_per_frame {0};
  std::array<std::uint8_t, 8> mapping {};
};

struct AudioRendererCapabilities {
  bool direct_submit {false};
  bool slow_decoder {false};
  bool arbitrary_audio_duration {false};
};

class AudioRenderer {
 public:
  virtual ~AudioRenderer() = default;
  [[nodiscard]] virtual AudioRendererCapabilities capabilities() const noexcept { return {}; }
  virtual int setup(const OpusStreamInformation& stream) noexcept = 0;
  virtual void start() noexcept = 0;
  virtual void stop() noexcept = 0;
  virtual void cleanup() noexcept = 0;
  virtual void submit(std::span<const std::byte> opus_sample) noexcept = 0;
};

class ConnectionListener {
 public:
  virtual ~ConnectionListener() = default;
  virtual void stage_starting(std::string_view) noexcept {}
  virtual void stage_complete(std::string_view) noexcept {}
  virtual void stage_failed(std::string_view, int) noexcept {}
  virtual void connection_started() noexcept {}
  virtual void connection_terminated(int) noexcept {}
  virtual void log_message(std::string_view) noexcept {}
  virtual void connection_status_changed(int) noexcept {}
  virtual void hdr_mode_changed(bool) noexcept {}
};

// moonlight-common-c currently owns process-global connection state, so only one
// StreamingSession may be active at a time. Renderer and listener callbacks can
// arrive from protocol threads and must remain alive until stop() completes.
class StreamingSession final {
 public:
  static constexpr int invalid_server_information = -10'000;
  static constexpr int another_session_is_active = -10'001;

  StreamingSession(
      ServerInformation server,
      StreamConfiguration& configuration,
      VideoRenderer& videoRenderer,
      AudioRenderer& audioRenderer,
      ConnectionListener& listener);
  ~StreamingSession();

  StreamingSession(const StreamingSession&) = delete;
  StreamingSession& operator=(const StreamingSession&) = delete;
  StreamingSession(StreamingSession&&) = delete;
  StreamingSession& operator=(StreamingSession&&) = delete;

  // Starts the protocol connection and returns once its initialization stages
  // complete. A zero result means the streaming threads are running.
  [[nodiscard]] int start();
  void interrupt() noexcept;
  void stop() noexcept;
  [[nodiscard]] bool active() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace one::protocol
