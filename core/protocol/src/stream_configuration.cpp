#include "one/protocol/stream_configuration.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

extern "C" {
#include "Limelight.h"
#include "PlatformCrypto.h"
}

namespace one::protocol {
namespace {

int native_location(const StreamLocation location) noexcept {
  switch (location) {
    case StreamLocation::automatic:
      return STREAM_CFG_AUTO;
    case StreamLocation::local:
      return STREAM_CFG_LOCAL;
    case StreamLocation::remote:
      return STREAM_CFG_REMOTE;
  }

  return STREAM_CFG_AUTO;
}

int native_audio_layout(const AudioLayout layout) noexcept {
  switch (layout) {
    case AudioLayout::stereo:
      return AUDIO_CONFIGURATION_STEREO;
    case AudioLayout::surround_5_1:
      return AUDIO_CONFIGURATION_51_SURROUND;
    case AudioLayout::surround_7_1:
      return AUDIO_CONFIGURATION_71_SURROUND;
  }

  return AUDIO_CONFIGURATION_STEREO;
}

int native_color_space(const ColorSpace color_space) noexcept {
  switch (color_space) {
    case ColorSpace::rec_601:
      return COLORSPACE_REC_601;
    case ColorSpace::rec_709:
      return COLORSPACE_REC_709;
    case ColorSpace::rec_2020:
      return COLORSPACE_REC_2020;
  }

  return COLORSPACE_REC_709;
}

int native_color_range(const ColorRange color_range) noexcept {
  switch (color_range) {
    case ColorRange::limited:
      return COLOR_RANGE_LIMITED;
    case ColorRange::full:
      return COLOR_RANGE_FULL;
  }

  return COLOR_RANGE_LIMITED;
}

int native_video_formats(const VideoFormats& formats) noexcept {
  int native_formats = 0;
  native_formats |= formats.h264 ? VIDEO_FORMAT_H264 : 0;
  native_formats |= formats.hevc ? VIDEO_FORMAT_H265 : 0;
  native_formats |= formats.hevc_10_bit ? VIDEO_FORMAT_H265_MAIN10 : 0;
  native_formats |= formats.av1 ? VIDEO_FORMAT_AV1_MAIN8 : 0;
  native_formats |= formats.av1_10_bit ? VIDEO_FORMAT_AV1_MAIN10 : 0;
  return native_formats;
}

std::string hex_string(const char* data, const std::size_t size) {
  constexpr std::array<char, 16> digits {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string encoded(size * 2, '0');
  for (std::size_t index = 0; index < size; ++index) {
    const auto value = static_cast<unsigned char>(data[index]);
    encoded[index * 2] = digits[value >> 4U];
    encoded[index * 2 + 1] = digits[value & 0x0FU];
  }
  return encoded;
}

void validate(const StreamSettings& settings) {
  if (settings.width <= 0 || settings.height <= 0) {
    throw std::invalid_argument("stream dimensions must be positive");
  }
  if (settings.frames_per_second <= 0) {
    throw std::invalid_argument("stream frame rate must be positive");
  }
  if (settings.bitrate_kbps <= 0) {
    throw std::invalid_argument("stream bitrate must be positive");
  }
  if (settings.packet_size <= 0) {
    throw std::invalid_argument("stream packet size must be positive");
  }
  if (native_video_formats(settings.video_formats) == 0) {
    throw std::invalid_argument("at least one video format must be enabled");
  }
}

}  // namespace

struct StreamConfiguration::Impl {
  explicit Impl(StreamSettings requested_settings) : settings(std::move(requested_settings)) {
    validate(settings);

    LiInitializeStreamConfiguration(&native);
    native.width = settings.width;
    native.height = settings.height;
    native.fps = settings.frames_per_second;
    native.bitrate = settings.bitrate_kbps;
    native.packetSize = settings.packet_size;
    native.streamingRemotely = native_location(settings.location);
    native.audioConfiguration = native_audio_layout(settings.audio_layout);
    native.supportedVideoFormats = native_video_formats(settings.video_formats);
    native.clientRefreshRateX100 = settings.client_refresh_rate_x100;
    native.colorSpace = native_color_space(settings.color_space);
    native.colorRange = native_color_range(settings.color_range);
    native.encryptionFlags = (settings.encrypt_audio ? ENCFLG_AUDIO : ENCFLG_NONE) |
                             (settings.encrypt_video ? ENCFLG_VIDEO : ENCFLG_NONE);
    PltGenerateRandomData(
        reinterpret_cast<unsigned char*>(native.remoteInputAesKey),
        static_cast<int>(sizeof(native.remoteInputAesKey)));
    // GameStream uses only the first four IV bytes as the remote-input key ID.
    PltGenerateRandomData(reinterpret_cast<unsigned char*>(native.remoteInputAesIv), 4);
  }

  StreamSettings settings;
  STREAM_CONFIGURATION native {};
};

StreamConfiguration::StreamConfiguration(StreamSettings settings)
    : impl_(std::make_unique<Impl>(std::move(settings))) {}

StreamConfiguration::~StreamConfiguration() = default;
StreamConfiguration::StreamConfiguration(StreamConfiguration&&) noexcept = default;
StreamConfiguration& StreamConfiguration::operator=(StreamConfiguration&&) noexcept = default;

const StreamSettings& StreamConfiguration::settings() const noexcept {
  return impl_->settings;
}

StreamLaunchParameters StreamConfiguration::launch_parameters() const {
  const auto& native = impl_->native;
  const auto* iv = reinterpret_cast<const unsigned char*>(native.remoteInputAesIv);
  const auto key_id = (static_cast<std::uint32_t>(iv[0]) << 24U) |
                      (static_cast<std::uint32_t>(iv[1]) << 16U) |
                      (static_cast<std::uint32_t>(iv[2]) << 8U) |
                      static_cast<std::uint32_t>(iv[3]);
  return {
      .width = native.width,
      .height = native.height,
      .frames_per_second = native.fps,
      .surround_audio_info = SURROUNDAUDIOINFO_FROM_AUDIO_CONFIGURATION(native.audioConfiguration),
      .remote_input_aes_key_hex = hex_string(native.remoteInputAesKey, sizeof(native.remoteInputAesKey)),
      .remote_input_aes_key_id = key_id,
      .hdr_requested = (native.supportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) != 0,
      .moonlight_query_parameters = launch_url_query_parameters(),
  };
}

void* StreamConfiguration::native_configuration() noexcept {
  return &impl_->native;
}

std::string launch_url_query_parameters() {
  const auto* parameters = LiGetLaunchUrlQueryParameters();
  return parameters == nullptr ? std::string {} : std::string {parameters};
}

}  // namespace one::protocol
