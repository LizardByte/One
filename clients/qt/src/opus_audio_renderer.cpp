#include "opus_audio_renderer.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include <QDebug>

#ifdef ONE_HAS_PC_MEDIA
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <opus/opus_multistream.h>
#endif

struct OpusAudioRenderer::Impl {
#ifdef ONE_HAS_PC_MEDIA
  OpusMSDecoder* decoder {nullptr};
  SDL_AudioStream* stream {nullptr};
  int channels {0};
  int samples_per_frame {0};
  std::vector<opus_int16> decoded_samples;
#endif
};

OpusAudioRenderer::OpusAudioRenderer() : impl_(std::make_unique<Impl>()) {}

OpusAudioRenderer::~OpusAudioRenderer() {
  cleanup();
}

bool OpusAudioRenderer::available() noexcept {
#ifdef ONE_HAS_PC_MEDIA
  return true;
#else
  return false;
#endif
}

one::protocol::AudioRendererCapabilities OpusAudioRenderer::capabilities() const noexcept {
  return {.arbitrary_audio_duration = true};
}

int OpusAudioRenderer::setup(const one::protocol::OpusStreamInformation& stream) noexcept {
  cleanup();
#ifdef ONE_HAS_PC_MEDIA
  if (stream.channel_count <= 0 || stream.channel_count > static_cast<int>(stream.mapping.size()) ||
      stream.sample_rate <= 0 || stream.samples_per_frame <= 0) {
    return -1;
  }

  int opus_error = OPUS_OK;
  impl_->decoder = opus_multistream_decoder_create(
      stream.sample_rate,
      stream.channel_count,
      stream.streams,
      stream.coupled_streams,
      stream.mapping.data(),
      &opus_error);
  if (impl_->decoder == nullptr || opus_error != OPUS_OK) {
    qWarning() << "Unable to create Opus decoder:" << opus_strerror(opus_error);
    cleanup();
    return opus_error == OPUS_OK ? -1 : opus_error;
  }

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    qWarning() << "Unable to initialize SDL audio:" << SDL_GetError();
    cleanup();
    return -1;
  }

  SDL_AudioSpec requested {};
  requested.freq = stream.sample_rate;
  requested.format = SDL_AUDIO_S16;
  requested.channels = stream.channel_count;
  impl_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested, nullptr, nullptr);
  if (impl_->stream == nullptr) {
    qWarning() << "Unable to open audio output:" << SDL_GetError();
    cleanup();
    return -1;
  }

  impl_->channels = stream.channel_count;
  impl_->samples_per_frame = stream.samples_per_frame;
  impl_->decoded_samples.resize(
      static_cast<std::size_t>(stream.channel_count) * static_cast<std::size_t>(stream.samples_per_frame));
  return 0;
#else
  static_cast<void>(stream);
  return -1;
#endif
}

void OpusAudioRenderer::start() noexcept {
#ifdef ONE_HAS_PC_MEDIA
  if (impl_->stream != nullptr && !SDL_ResumeAudioStreamDevice(impl_->stream)) {
    qWarning() << "Unable to resume audio output:" << SDL_GetError();
  }
#endif
}

void OpusAudioRenderer::stop() noexcept {
#ifdef ONE_HAS_PC_MEDIA
  if (impl_->stream != nullptr) {
    if (!SDL_PauseAudioStreamDevice(impl_->stream)) {
      qWarning() << "Unable to pause audio output:" << SDL_GetError();
    }
    if (!SDL_ClearAudioStream(impl_->stream)) {
      qWarning() << "Unable to clear queued audio:" << SDL_GetError();
    }
  }
#endif
}

void OpusAudioRenderer::cleanup() noexcept {
#ifdef ONE_HAS_PC_MEDIA
  if (impl_->stream != nullptr) {
    SDL_DestroyAudioStream(impl_->stream);
    impl_->stream = nullptr;
  }
  if (impl_->decoder != nullptr) {
    opus_multistream_decoder_destroy(impl_->decoder);
    impl_->decoder = nullptr;
  }
  impl_->decoded_samples.clear();
  impl_->channels = 0;
  impl_->samples_per_frame = 0;
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
}

void OpusAudioRenderer::submit(const std::span<const std::byte> opus_sample) noexcept {
#ifdef ONE_HAS_PC_MEDIA
  if (impl_->decoder == nullptr || impl_->stream == nullptr || opus_sample.empty()) {
    return;
  }
  const auto samples = opus_multistream_decode(
      impl_->decoder,
      reinterpret_cast<const unsigned char*>(opus_sample.data()),
      static_cast<opus_int32>(opus_sample.size()),
      impl_->decoded_samples.data(),
      impl_->samples_per_frame,
      0);
  if (samples < 0) {
    qWarning() << "Unable to decode Opus audio packet:" << opus_strerror(samples);
    return;
  }
  const auto bytes = static_cast<int>(
      samples * impl_->channels * static_cast<int>(sizeof(opus_int16)));
  if (!SDL_PutAudioStreamData(impl_->stream, impl_->decoded_samples.data(), bytes)) {
    qWarning() << "Unable to queue decoded audio:" << SDL_GetError();
  }
#else
  static_cast<void>(opus_sample);
#endif
}
