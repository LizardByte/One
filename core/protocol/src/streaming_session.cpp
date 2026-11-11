#include "one/protocol/streaming_session.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <utility>

extern "C" {
#include "Limelight.h"
}

namespace one::protocol {
namespace {

VideoBufferType video_buffer_type(const int type) noexcept {
  switch (type) {
    case BUFFER_TYPE_PICDATA:
      return VideoBufferType::picture;
    case BUFFER_TYPE_SPS:
      return VideoBufferType::sequence_parameter_set;
    case BUFFER_TYPE_PPS:
      return VideoBufferType::picture_parameter_set;
    case BUFFER_TYPE_VPS:
      return VideoBufferType::video_parameter_set;
    default:
      return VideoBufferType::unknown;
  }
}

VideoFormat video_format(const int format) noexcept {
  switch (format) {
    case VIDEO_FORMAT_H264:
      return VideoFormat::h264;
    case VIDEO_FORMAT_H264_HIGH8_444:
      return VideoFormat::h264_444;
    case VIDEO_FORMAT_H265:
      return VideoFormat::hevc;
    case VIDEO_FORMAT_H265_MAIN10:
      return VideoFormat::hevc_10_bit;
    case VIDEO_FORMAT_H265_REXT8_444:
      return VideoFormat::hevc_444;
    case VIDEO_FORMAT_H265_REXT10_444:
      return VideoFormat::hevc_10_bit_444;
    case VIDEO_FORMAT_AV1_MAIN8:
      return VideoFormat::av1;
    case VIDEO_FORMAT_AV1_MAIN10:
      return VideoFormat::av1_10_bit;
    case VIDEO_FORMAT_AV1_HIGH8_444:
      return VideoFormat::av1_444;
    case VIDEO_FORMAT_AV1_HIGH10_444:
      return VideoFormat::av1_10_bit_444;
    default:
      return VideoFormat::unknown;
  }
}

ColorSpace color_space(const std::uint8_t color_space) noexcept {
  switch (color_space) {
    case COLORSPACE_REC_601:
      return ColorSpace::rec_601;
    case COLORSPACE_REC_2020:
      return ColorSpace::rec_2020;
    case COLORSPACE_REC_709:
    default:
      return ColorSpace::rec_709;
  }
}

AudioLayout audio_layout(const int configuration) noexcept {
  switch (configuration) {
    case AUDIO_CONFIGURATION_51_SURROUND:
      return AudioLayout::surround_5_1;
    case AUDIO_CONFIGURATION_71_SURROUND:
      return AudioLayout::surround_7_1;
    case AUDIO_CONFIGURATION_STEREO:
    default:
      return AudioLayout::stereo;
  }
}

int native_video_capabilities(const VideoRendererCapabilities& capabilities) noexcept {
  int native = capabilities.direct_submit ? CAPABILITY_DIRECT_SUBMIT : 0;
  native |= capabilities.reference_frame_invalidation_h264 ? CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC : 0;
  native |= capabilities.reference_frame_invalidation_hevc ? CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC : 0;
  native |= capabilities.reference_frame_invalidation_av1 ? CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1 : 0;
  native |= CAPABILITY_SLICES_PER_FRAME(capabilities.slices_per_frame);
  return native;
}

int native_audio_capabilities(const AudioRendererCapabilities& capabilities) noexcept {
  int native = capabilities.direct_submit ? CAPABILITY_DIRECT_SUBMIT : 0;
  native |= capabilities.slow_decoder ? CAPABILITY_SLOW_OPUS_DECODER : 0;
  native |= capabilities.arbitrary_audio_duration ? CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION : 0;
  return native;
}

int native_mouse_button(const MouseButton button) noexcept {
  switch (button) {
    case MouseButton::left:
      return BUTTON_LEFT;
    case MouseButton::middle:
      return BUTTON_MIDDLE;
    case MouseButton::right:
      return BUTTON_RIGHT;
    case MouseButton::extra_1:
      return BUTTON_X1;
    case MouseButton::extra_2:
      return BUTTON_X2;
  }
  return 0;
}

char native_key_modifiers(const KeyModifiers modifiers) noexcept {
  char native = 0;
  native |= modifiers.shift ? MODIFIER_SHIFT : 0;
  native |= modifiers.control ? MODIFIER_CTRL : 0;
  native |= modifiers.alt ? MODIFIER_ALT : 0;
  native |= modifiers.meta ? MODIFIER_META : 0;
  return native;
}

int native_controller_type(const ControllerType type) noexcept {
  switch (type) {
    case ControllerType::xbox:
      return LI_CTYPE_XBOX;
    case ControllerType::playstation:
      return LI_CTYPE_PS;
    case ControllerType::nintendo:
      return LI_CTYPE_NINTENDO;
    case ControllerType::steam:
      return LI_CTYPE_STEAM;
    case ControllerType::unknown:
      return LI_CTYPE_UNKNOWN;
  }
  return LI_CTYPE_UNKNOWN;
}

int native_controller_buttons(const ControllerButtonMask buttons) noexcept {
  const auto has = [buttons](const ControllerButton button) {
    return (buttons & controller_button(button)) != 0;
  };
  int native = 0;
  native |= has(ControllerButton::dpad_up) ? UP_FLAG : 0;
  native |= has(ControllerButton::dpad_down) ? DOWN_FLAG : 0;
  native |= has(ControllerButton::dpad_left) ? LEFT_FLAG : 0;
  native |= has(ControllerButton::dpad_right) ? RIGHT_FLAG : 0;
  native |= has(ControllerButton::start) ? PLAY_FLAG : 0;
  native |= has(ControllerButton::back) ? BACK_FLAG : 0;
  native |= has(ControllerButton::left_stick) ? LS_CLK_FLAG : 0;
  native |= has(ControllerButton::right_stick) ? RS_CLK_FLAG : 0;
  native |= has(ControllerButton::left_shoulder) ? LB_FLAG : 0;
  native |= has(ControllerButton::right_shoulder) ? RB_FLAG : 0;
  native |= has(ControllerButton::guide) ? SPECIAL_FLAG : 0;
  native |= has(ControllerButton::a) ? A_FLAG : 0;
  native |= has(ControllerButton::b) ? B_FLAG : 0;
  native |= has(ControllerButton::x) ? X_FLAG : 0;
  native |= has(ControllerButton::y) ? Y_FLAG : 0;
  native |= has(ControllerButton::paddle_1) ? PADDLE1_FLAG : 0;
  native |= has(ControllerButton::paddle_2) ? PADDLE2_FLAG : 0;
  native |= has(ControllerButton::paddle_3) ? PADDLE3_FLAG : 0;
  native |= has(ControllerButton::paddle_4) ? PADDLE4_FLAG : 0;
  native |= has(ControllerButton::touchpad) ? TOUCHPAD_FLAG : 0;
  native |= has(ControllerButton::misc) ? MISC_FLAG : 0;
  return native;
}

int native_controller_capabilities(const ControllerCapabilities capabilities) noexcept {
  int native = 0;
  native |= capabilities.analog_triggers ? LI_CCAP_ANALOG_TRIGGERS : 0;
  native |= capabilities.rumble ? LI_CCAP_RUMBLE : 0;
  native |= capabilities.trigger_rumble ? LI_CCAP_TRIGGER_RUMBLE : 0;
  native |= capabilities.touchpad ? LI_CCAP_TOUCHPAD : 0;
  native |= capabilities.dual_touchpad ? LI_CCAP_DUAL_TOUCHPAD : 0;
  native |= capabilities.accelerometer ? LI_CCAP_ACCEL : 0;
  native |= capabilities.gyroscope ? LI_CCAP_GYRO : 0;
  native |= capabilities.battery_state ? LI_CCAP_BATTERY_STATE : 0;
  native |= capabilities.rgb_led ? LI_CCAP_RGB_LED : 0;
  return native;
}

int native_controller_touch_action(const ControllerTouchAction action) noexcept {
  switch (action) {
    case ControllerTouchAction::down:
      return LI_TOUCH_EVENT_DOWN;
    case ControllerTouchAction::up:
      return LI_TOUCH_EVENT_UP;
    case ControllerTouchAction::move:
      return LI_TOUCH_EVENT_MOVE;
    case ControllerTouchAction::cancel:
      return LI_TOUCH_EVENT_CANCEL;
  }
  return LI_TOUCH_EVENT_CANCEL;
}

int native_controller_motion_type(const ControllerMotionType type) noexcept {
  switch (type) {
    case ControllerMotionType::accelerometer:
      return LI_MOTION_TYPE_ACCEL;
    case ControllerMotionType::gyroscope:
      return LI_MOTION_TYPE_GYRO;
  }
  return LI_MOTION_TYPE_ACCEL;
}

int native_controller_battery_state(const ControllerBatteryState state) noexcept {
  switch (state) {
    case ControllerBatteryState::unknown:
      return LI_BATTERY_STATE_UNKNOWN;
    case ControllerBatteryState::not_present:
      return LI_BATTERY_STATE_NOT_PRESENT;
    case ControllerBatteryState::discharging:
      return LI_BATTERY_STATE_DISCHARGING;
    case ControllerBatteryState::charging:
      return LI_BATTERY_STATE_CHARGING;
    case ControllerBatteryState::not_charging:
      return LI_BATTERY_STATE_NOT_CHARGING;
    case ControllerBatteryState::full:
      return LI_BATTERY_STATE_FULL;
  }
  return LI_BATTERY_STATE_UNKNOWN;
}

std::string_view stage_name(const int stage) noexcept {
  const auto* name = LiGetStageName(stage);
  return name == nullptr ? std::string_view {} : std::string_view {name};
}

}  // namespace

EncodedVideoFrameView::EncodedVideoFrameView(const void* native) noexcept : native_(native) {}

int EncodedVideoFrameView::frame_number() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->frameNumber;
}

bool EncodedVideoFrameView::key_frame() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->frameType == FRAME_TYPE_IDR;
}

std::uint16_t EncodedVideoFrameView::host_processing_latency() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->frameHostProcessingLatency;
}

std::uint64_t EncodedVideoFrameView::receive_time_us() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->receiveTimeUs;
}

std::uint64_t EncodedVideoFrameView::enqueue_time_us() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->enqueueTimeUs;
}

std::uint64_t EncodedVideoFrameView::presentation_time_us() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->presentationTimeUs;
}

std::uint32_t EncodedVideoFrameView::rtp_timestamp() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->rtpTimestamp;
}

std::size_t EncodedVideoFrameView::data_size() const noexcept {
  return static_cast<std::size_t>(static_cast<const DECODE_UNIT*>(native_)->fullLength);
}

bool EncodedVideoFrameView::hdr_active() const noexcept {
  return static_cast<const DECODE_UNIT*>(native_)->hdrActive;
}

ColorSpace EncodedVideoFrameView::color_space() const noexcept {
  return one::protocol::color_space(static_cast<const DECODE_UNIT*>(native_)->colorspace);
}

std::size_t EncodedVideoFrameView::buffer_count() const noexcept {
  std::size_t count = 0;
  for (auto* entry = static_cast<const DECODE_UNIT*>(native_)->bufferList; entry != nullptr; entry = entry->next) {
    ++count;
  }
  return count;
}

EncodedVideoBufferView EncodedVideoFrameView::buffer(const std::size_t index) const noexcept {
  auto* entry = static_cast<const DECODE_UNIT*>(native_)->bufferList;
  for (std::size_t current = 0; entry != nullptr && current < index; ++current) {
    entry = entry->next;
  }
  if (entry == nullptr) {
    return {};
  }
  return {
      .bytes = std::span<const std::byte> {
          reinterpret_cast<const std::byte*>(entry->data),
          static_cast<std::size_t>(entry->length)},
      .type = video_buffer_type(entry->bufferType),
  };
}

struct StreamingSession::Impl {
  Impl(
      ServerInformation requested_server,
      StreamConfiguration& requested_configuration,
      VideoRenderer& requested_video,
      AudioRenderer& requested_audio,
      ConnectionListener& requested_listener)
      : server(std::move(requested_server)),
        configuration(requested_configuration),
        video(requested_video),
        audio(requested_audio),
        listener(requested_listener) {}

  static Impl* current() noexcept {
    return owner.load(std::memory_order_acquire);
  }

  static void stage_starting(const int stage) {
    if (auto* session = current(); session != nullptr) {
      session->listener.stage_starting(stage_name(stage));
    }
  }

  static void stage_complete(const int stage) {
    if (auto* session = current(); session != nullptr) {
      session->listener.stage_complete(stage_name(stage));
    }
  }

  static void stage_failed(const int stage, const int error_code) {
    if (auto* session = current(); session != nullptr) {
      session->listener.stage_failed(stage_name(stage), error_code);
    }
  }

  static void connection_started() {
    if (auto* session = current(); session != nullptr) {
      session->running.store(true, std::memory_order_release);
      session->listener.connection_started();
    }
  }

  static void connection_terminated(const int error_code) {
    if (auto* session = current(); session != nullptr) {
      session->running.store(false, std::memory_order_release);
      session->listener.connection_terminated(error_code);
    }
  }

  static void log_message(const char* format, ...) {
    auto* session = current();
    if (session == nullptr || format == nullptr) {
      return;
    }
    char message[2048] {};
    va_list arguments;
    va_start(arguments, format);
    static_cast<void>(std::vsnprintf(message, sizeof(message), format, arguments));
    va_end(arguments);
    session->listener.log_message(message);
  }

  static void connection_status_changed(const int status) {
    if (auto* session = current(); session != nullptr) {
      session->listener.connection_status_changed(status);
    }
  }

  static void hdr_mode_changed(const bool enabled) {
    if (auto* session = current(); session != nullptr) {
      session->listener.hdr_mode_changed(enabled);
    }
  }

  static void controller_rumble(
      const std::uint16_t controller_number,
      const std::uint16_t low_frequency_motor,
      const std::uint16_t high_frequency_motor) {
    if (auto* session = current(); session != nullptr) {
      session->listener.controller_rumble(
          controller_number, low_frequency_motor, high_frequency_motor);
    }
  }

  static void controller_trigger_rumble(
      const std::uint16_t controller_number,
      const std::uint16_t left_trigger_motor,
      const std::uint16_t right_trigger_motor) {
    if (auto* session = current(); session != nullptr) {
      session->listener.controller_trigger_rumble(
          controller_number, left_trigger_motor, right_trigger_motor);
    }
  }

  static void controller_motion_state(
      const std::uint16_t controller_number,
      const std::uint8_t motion_type,
      const std::uint16_t report_rate_hz) {
    auto* session = current();
    if (session == nullptr) {
      return;
    }
    switch (motion_type) {
      case LI_MOTION_TYPE_ACCEL:
        session->listener.controller_motion_state(
            controller_number, ControllerMotionType::accelerometer, report_rate_hz);
        break;
      case LI_MOTION_TYPE_GYRO:
        session->listener.controller_motion_state(
            controller_number, ControllerMotionType::gyroscope, report_rate_hz);
        break;
      default:
        break;
    }
  }

  static void controller_led(
      const std::uint16_t controller_number,
      const std::uint8_t red,
      const std::uint8_t green,
      const std::uint8_t blue) {
    if (auto* session = current(); session != nullptr) {
      session->listener.controller_led(controller_number, red, green, blue);
    }
  }

  static void controller_adaptive_triggers(
      const std::uint16_t controller_number,
      const std::uint8_t event_flags,
      const std::uint8_t left_type,
      const std::uint8_t right_type,
      std::uint8_t* left_payload,
      std::uint8_t* right_payload) {
    auto* session = current();
    if (session == nullptr) {
      return;
    }
    ControllerAdaptiveTriggerEffect effect {
        .update_left = (event_flags & DS_EFFECT_LEFT_TRIGGER) != 0,
        .update_right = (event_flags & DS_EFFECT_RIGHT_TRIGGER) != 0,
        .left_type = left_type,
        .right_type = right_type,
    };
    if (left_payload != nullptr) {
      std::copy_n(left_payload, effect.left_payload.size(), effect.left_payload.begin());
    }
    if (right_payload != nullptr) {
      std::copy_n(right_payload, effect.right_payload.size(), effect.right_payload.begin());
    }
    session->listener.controller_adaptive_triggers(controller_number, effect);
  }

  static int video_setup(
      const int format,
      const int width,
      const int height,
      const int redraw_rate,
      void*,
      const int) {
    auto* session = current();
    return session == nullptr
             ? -1
             : session->video.setup({
                   .format = video_format(format),
                   .width = width,
                   .height = height,
                   .redraw_rate = redraw_rate,
               });
  }

  static void video_start() {
    if (auto* session = current(); session != nullptr) {
      session->video.start();
    }
  }

  static void video_stop() {
    if (auto* session = current(); session != nullptr) {
      session->video.stop();
    }
  }

  static void video_cleanup() {
    if (auto* session = current(); session != nullptr) {
      session->video.cleanup();
    }
  }

  static int video_submit(PDECODE_UNIT decode_unit) {
    auto* session = current();
    if (session == nullptr || decode_unit == nullptr) {
      return DR_NEED_IDR;
    }
    return static_cast<int>(session->video.submit(EncodedVideoFrameView(decode_unit)));
  }

  static int audio_setup(
      const int audio_configuration,
      const POPUS_MULTISTREAM_CONFIGURATION opus,
      void*,
      const int) {
    auto* session = current();
    if (session == nullptr || opus == nullptr) {
      return -1;
    }
    OpusStreamInformation stream {
        .layout = audio_layout(audio_configuration),
        .sample_rate = opus->sampleRate,
        .channel_count = opus->channelCount,
        .streams = opus->streams,
        .coupled_streams = opus->coupledStreams,
        .samples_per_frame = opus->samplesPerFrame,
    };
    for (std::size_t index = 0; index < stream.mapping.size(); ++index) {
      stream.mapping[index] = opus->mapping[index];
    }
    return session->audio.setup(stream);
  }

  static void audio_start() {
    if (auto* session = current(); session != nullptr) {
      session->audio.start();
    }
  }

  static void audio_stop() {
    if (auto* session = current(); session != nullptr) {
      session->audio.stop();
    }
  }

  static void audio_cleanup() {
    if (auto* session = current(); session != nullptr) {
      session->audio.cleanup();
    }
  }

  static void audio_submit(char* sample, const int length) {
    if (auto* session = current(); session != nullptr && sample != nullptr && length > 0) {
      session->audio.submit(std::span<const std::byte> {
          reinterpret_cast<const std::byte*>(sample),
          static_cast<std::size_t>(length)});
    }
  }

  ServerInformation server;
  StreamConfiguration& configuration;
  VideoRenderer& video;
  AudioRenderer& audio;
  ConnectionListener& listener;
  std::atomic_bool running {false};
  bool owns_connection {false};

  static std::atomic<Impl*> owner;
};

std::atomic<StreamingSession::Impl*> StreamingSession::Impl::owner {nullptr};

StreamingSession::StreamingSession(
    ServerInformation server,
    StreamConfiguration& configuration,
    VideoRenderer& video_renderer,
    AudioRenderer& audio_renderer,
    ConnectionListener& listener)
    : impl_(std::make_unique<Impl>(
          std::move(server),
          configuration,
          video_renderer,
          audio_renderer,
          listener)) {}

StreamingSession::~StreamingSession() {
  stop();
}

int StreamingSession::start() {
  if (impl_->server.address.empty() || impl_->server.app_version.empty()) {
    return invalid_server_information;
  }

  Impl* expected = nullptr;
  if (!Impl::owner.compare_exchange_strong(expected, impl_.get(), std::memory_order_acq_rel)) {
    return another_session_is_active;
  }
  impl_->owns_connection = true;

  SERVER_INFORMATION server {};
  LiInitializeServerInformation(&server);
  server.address = impl_->server.address.c_str();
  server.serverInfoAppVersion = impl_->server.app_version.c_str();
  server.serverInfoGfeVersion = impl_->server.gfe_version.empty() ? nullptr : impl_->server.gfe_version.c_str();
  server.rtspSessionUrl = impl_->server.rtsp_session_url.empty() ? nullptr : impl_->server.rtsp_session_url.c_str();
  server.serverCodecModeSupport = impl_->server.codec_mode_support;

  CONNECTION_LISTENER_CALLBACKS listener {};
  LiInitializeConnectionCallbacks(&listener);
  listener.stageStarting = Impl::stage_starting;
  listener.stageComplete = Impl::stage_complete;
  listener.stageFailed = Impl::stage_failed;
  listener.connectionStarted = Impl::connection_started;
  listener.connectionTerminated = Impl::connection_terminated;
  listener.logMessage = Impl::log_message;
  listener.connectionStatusUpdate = Impl::connection_status_changed;
  listener.setHdrMode = Impl::hdr_mode_changed;
  listener.rumble = Impl::controller_rumble;
  listener.rumbleTriggers = Impl::controller_trigger_rumble;
  listener.setMotionEventState = Impl::controller_motion_state;
  listener.setControllerLED = Impl::controller_led;
  listener.setAdaptiveTriggers = Impl::controller_adaptive_triggers;

  DECODER_RENDERER_CALLBACKS video {};
  LiInitializeVideoCallbacks(&video);
  video.setup = Impl::video_setup;
  video.start = Impl::video_start;
  video.stop = Impl::video_stop;
  video.cleanup = Impl::video_cleanup;
  video.submitDecodeUnit = Impl::video_submit;
  video.capabilities = native_video_capabilities(impl_->video.capabilities());

  AUDIO_RENDERER_CALLBACKS audio {};
  LiInitializeAudioCallbacks(&audio);
  audio.init = Impl::audio_setup;
  audio.start = Impl::audio_start;
  audio.stop = Impl::audio_stop;
  audio.cleanup = Impl::audio_cleanup;
  audio.decodeAndPlaySample = Impl::audio_submit;
  audio.capabilities = native_audio_capabilities(impl_->audio.capabilities());

  const auto result = LiStartConnection(
      &server,
      static_cast<PSTREAM_CONFIGURATION>(impl_->configuration.native_configuration()),
      &listener,
      &video,
      &audio,
      nullptr,
      0,
      nullptr,
      0);
  if (result != 0) {
    impl_->running.store(false, std::memory_order_release);
    impl_->owns_connection = false;
    Impl::owner.store(nullptr, std::memory_order_release);
  }
  return result;
}

void StreamingSession::interrupt() noexcept {
  if (impl_->owns_connection && Impl::current() == impl_.get()) {
    LiInterruptConnection();
  }
}

void StreamingSession::stop() noexcept {
  if (!impl_->owns_connection || Impl::current() != impl_.get()) {
    return;
  }
  LiStopConnection();
  impl_->running.store(false, std::memory_order_release);
  impl_->owns_connection = false;
  Impl::owner.store(nullptr, std::memory_order_release);
}

bool StreamingSession::active() const noexcept {
  return impl_->running.load(std::memory_order_acquire);
}

int StreamingSession::send_mouse_move(const std::int16_t delta_x, const std::int16_t delta_y) noexcept {
  return active() ? LiSendMouseMoveEvent(delta_x, delta_y) : input_not_ready;
}

int StreamingSession::send_mouse_button(const MouseButton button, const ButtonAction action) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  const auto native_button = native_mouse_button(button);
  if (native_button == 0) {
    return invalid_input;
  }
  return LiSendMouseButtonEvent(
      action == ButtonAction::press ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE,
      native_button);
}

int StreamingSession::send_vertical_scroll(const std::int16_t amount) noexcept {
  return active() ? LiSendHighResScrollEvent(amount) : input_not_ready;
}

int StreamingSession::send_horizontal_scroll(const std::int16_t amount) noexcept {
  return active() ? LiSendHighResHScrollEvent(amount) : input_not_ready;
}

int StreamingSession::send_key(
    const std::uint16_t virtual_key,
    const KeyAction action,
    const KeyModifiers modifiers,
    const bool normalized) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (virtual_key == 0 || virtual_key > 0xFFU) {
    return invalid_input;
  }
  const auto protocol_key = static_cast<short>(0x8000U | virtual_key);
  return LiSendKeyboardEvent2(
      protocol_key,
      action == KeyAction::press ? KEY_ACTION_DOWN : KEY_ACTION_UP,
      native_key_modifiers(modifiers),
      normalized ? 0 : SS_KBE_FLAG_NON_NORMALIZED);
}

int StreamingSession::send_utf8_text(const std::string_view text) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (text.empty() || text.size() > std::numeric_limits<unsigned int>::max()) {
    return invalid_input;
  }
  return LiSendUtf8TextEvent(text.data(), static_cast<unsigned int>(text.size()));
}

int StreamingSession::send_controller_arrival(
    const std::uint8_t controller_number,
    const std::uint16_t active_controller_mask,
    const ControllerType type,
    const ControllerButtonMask supported_buttons,
    const ControllerCapabilities capabilities) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (controller_number >= 16 || (active_controller_mask & (1U << controller_number)) == 0) {
    return invalid_input;
  }
  return LiSendControllerArrivalEvent(
      controller_number,
      active_controller_mask,
      static_cast<std::uint8_t>(native_controller_type(type)),
      static_cast<std::uint32_t>(native_controller_buttons(supported_buttons)),
      static_cast<std::uint16_t>(native_controller_capabilities(capabilities)));
}

int StreamingSession::send_controller_state(
    const std::uint8_t controller_number,
    const std::uint16_t active_controller_mask,
    const ControllerState& state) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (controller_number >= 16 || (active_controller_mask & (1U << controller_number)) == 0) {
    return invalid_input;
  }
  return LiSendMultiControllerEvent(
      controller_number,
      active_controller_mask,
      native_controller_buttons(state.buttons),
      state.left_trigger,
      state.right_trigger,
      state.left_stick_x,
      state.left_stick_y,
      state.right_stick_x,
      state.right_stick_y);
}

int StreamingSession::send_controller_departure(
    const std::uint8_t controller_number,
    const std::uint16_t active_controller_mask) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (controller_number >= 16 || (active_controller_mask & (1U << controller_number)) != 0) {
    return invalid_input;
  }
  return LiSendMultiControllerEvent(controller_number, active_controller_mask, 0, 0, 0, 0, 0, 0, 0);
}

int StreamingSession::send_controller_touch(
    const std::uint8_t controller_number,
    const std::uint8_t touchpad_index,
    const std::uint32_t pointer_id,
    const ControllerTouchAction action,
    const float x,
    const float y,
    const float pressure) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (controller_number >= 16 || touchpad_index >= 2) {
    return invalid_input;
  }
  if (action != ControllerTouchAction::cancel &&
      (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(pressure) || x < 0.0F ||
       x > 1.0F || y < 0.0F || y > 1.0F || pressure < 0.0F || pressure > 1.0F)) {
    return invalid_input;
  }
  return LiSendControllerTouchEvent2(
      controller_number,
      static_cast<std::uint8_t>(native_controller_touch_action(action)),
      touchpad_index,
      pointer_id,
      x,
      y,
      pressure);
}

int StreamingSession::send_controller_motion(
    const std::uint8_t controller_number,
    const ControllerMotionType type,
    const float x,
    const float y,
    const float z) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (controller_number >= 16 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
    return invalid_input;
  }
  return LiSendControllerMotionEvent(
      controller_number,
      static_cast<std::uint8_t>(native_controller_motion_type(type)),
      x,
      y,
      z);
}

int StreamingSession::send_controller_battery(
    const std::uint8_t controller_number,
    const ControllerBattery battery) noexcept {
  if (!active()) {
    return input_not_ready;
  }
  if (controller_number >= 16 ||
      (battery.percentage > 100 && battery.percentage != ControllerBattery::unknown_percentage)) {
    return invalid_input;
  }
  return LiSendControllerBatteryEvent(
      controller_number,
      static_cast<std::uint8_t>(native_controller_battery_state(battery.state)),
      battery.percentage);
}

}  // namespace one::protocol
