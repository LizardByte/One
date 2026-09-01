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

enum class MouseButton {
  left,
  middle,
  right,
  extra_1,
  extra_2,
};

enum class ButtonAction {
  press,
  release,
};

enum class KeyAction {
  press,
  release,
};

struct KeyModifiers {
  bool shift {false};
  bool control {false};
  bool alt {false};
  bool meta {false};
};

enum class ControllerType {
  unknown,
  xbox,
  playstation,
  nintendo,
  steam,
};

enum class ControllerButton : std::uint32_t {
  dpad_up = 1U << 0U,
  dpad_down = 1U << 1U,
  dpad_left = 1U << 2U,
  dpad_right = 1U << 3U,
  start = 1U << 4U,
  back = 1U << 5U,
  left_stick = 1U << 6U,
  right_stick = 1U << 7U,
  left_shoulder = 1U << 8U,
  right_shoulder = 1U << 9U,
  guide = 1U << 10U,
  a = 1U << 11U,
  b = 1U << 12U,
  x = 1U << 13U,
  y = 1U << 14U,
  paddle_1 = 1U << 15U,
  paddle_2 = 1U << 16U,
  paddle_3 = 1U << 17U,
  paddle_4 = 1U << 18U,
  touchpad = 1U << 19U,
  misc = 1U << 20U,
};

using ControllerButtonMask = std::uint32_t;

[[nodiscard]] constexpr ControllerButtonMask controller_button(const ControllerButton button) noexcept {
  return static_cast<ControllerButtonMask>(button);
}

struct ControllerCapabilities {
  bool analog_triggers {false};
  bool rumble {false};
  bool trigger_rumble {false};
  bool touchpad {false};
  bool dual_touchpad {false};
  bool accelerometer {false};
  bool gyroscope {false};
  bool battery_state {false};
  bool rgb_led {false};
};

struct ControllerState {
  ControllerButtonMask buttons {0};
  std::uint8_t left_trigger {0};
  std::uint8_t right_trigger {0};
  std::int16_t left_stick_x {0};
  std::int16_t left_stick_y {0};
  std::int16_t right_stick_x {0};
  std::int16_t right_stick_y {0};
};

enum class ControllerTouchAction {
  down,
  up,
  move,
  cancel,
};

enum class ControllerMotionType {
  accelerometer,
  gyroscope,
};

enum class ControllerBatteryState {
  unknown,
  not_present,
  discharging,
  charging,
  not_charging,
  full,
};

struct ControllerBattery {
  static constexpr std::uint8_t unknown_percentage = 0xFF;

  ControllerBatteryState state {ControllerBatteryState::unknown};
  std::uint8_t percentage {unknown_percentage};
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
  virtual void controller_rumble(std::uint16_t, std::uint16_t, std::uint16_t) noexcept {}
  virtual void controller_trigger_rumble(std::uint16_t, std::uint16_t, std::uint16_t) noexcept {}
  virtual void controller_motion_state(
      std::uint16_t,
      ControllerMotionType,
      std::uint16_t) noexcept {}
};

// moonlight-common-c currently owns process-global connection state, so only one
// StreamingSession may be active at a time. Renderer and listener callbacks can
// arrive from protocol threads and must remain alive until stop() completes.
class StreamingSession final {
 public:
  static constexpr int invalid_server_information = -10'000;
  static constexpr int another_session_is_active = -10'001;
  static constexpr int input_not_ready = -10'002;
  static constexpr int invalid_input = -10'003;

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

  // GameStream keyboard packets use Win32 virtual-key values independent of
  // the client platform. The protocol prefix is added by this boundary.
  [[nodiscard]] int send_mouse_move(std::int16_t deltaX, std::int16_t deltaY) noexcept;
  [[nodiscard]] int send_mouse_button(MouseButton button, ButtonAction action) noexcept;
  [[nodiscard]] int send_vertical_scroll(std::int16_t amount) noexcept;
  [[nodiscard]] int send_horizontal_scroll(std::int16_t amount) noexcept;
  [[nodiscard]] int send_key(
      std::uint16_t virtualKey,
      KeyAction action,
      KeyModifiers modifiers,
      bool normalized = true) noexcept;
  [[nodiscard]] int send_utf8_text(std::string_view text) noexcept;
  [[nodiscard]] int send_controller_arrival(
      std::uint8_t controllerNumber,
      std::uint16_t activeControllerMask,
      ControllerType type,
      ControllerButtonMask supportedButtons,
      ControllerCapabilities capabilities) noexcept;
  [[nodiscard]] int send_controller_state(
      std::uint8_t controllerNumber,
      std::uint16_t activeControllerMask,
      const ControllerState& state) noexcept;
  [[nodiscard]] int send_controller_departure(
      std::uint8_t controllerNumber,
      std::uint16_t activeControllerMask) noexcept;
  [[nodiscard]] int send_controller_touch(
      std::uint8_t controllerNumber,
      std::uint8_t touchpadIndex,
      std::uint32_t pointerId,
      ControllerTouchAction action,
      float x,
      float y,
      float pressure) noexcept;
  [[nodiscard]] int send_controller_motion(
      std::uint8_t controllerNumber,
      ControllerMotionType type,
      float x,
      float y,
      float z) noexcept;
  [[nodiscard]] int send_controller_battery(
      std::uint8_t controllerNumber,
      ControllerBattery battery) noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace one::protocol
