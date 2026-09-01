#include "opus_audio_renderer.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include <QDebug>

#ifdef ONE_HAS_PC_MEDIA
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL2/SDL.h>
#include <opus/opus_multistream.h>
#endif

struct OpusAudioRenderer::Impl {
#ifdef ONE_HAS_PC_MEDIA
  OpusMSDecoder* decoder {nullptr};
  SDL_AudioDeviceID device {0};
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

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    qWarning() << "Unable to initialize SDL audio:" << SDL_GetError();
    cleanup();
    return -1;
  }

  SDL_AudioSpec requested {};
  requested.freq = stream.sample_rate;
  requested.format = AUDIO_S16SYS;
  requested.channels = static_cast<Uint8>(stream.channel_count);
  requested.samples = 512;
  impl_->device = SDL_OpenAudioDevice(nullptr, 0, &requested, nullptr, 0);
  if (impl_->device == 0) {
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
  if (impl_->device != 0) {
    SDL_PauseAudioDevice(impl_->device, 0);
  }
#endif
}

void OpusAudioRenderer::stop() noexcept {
#ifdef ONE_HAS_PC_MEDIA
  if (impl_->device != 0) {
    SDL_PauseAudioDevice(impl_->device, 1);
    SDL_ClearQueuedAudio(impl_->device);
  }
#endif
}

void OpusAudioRenderer::cleanup() noexcept {
#ifdef ONE_HAS_PC_MEDIA
  if (impl_->device != 0) {
    SDL_CloseAudioDevice(impl_->device);
    impl_->device = 0;
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
  if (impl_->decoder == nullptr || impl_->device == 0 || opus_sample.empty()) {
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
  const auto bytes = static_cast<Uint32>(
      samples * impl_->channels * static_cast<int>(sizeof(opus_int16)));
  if (SDL_QueueAudio(impl_->device, impl_->decoded_samples.data(), bytes) != 0) {
    qWarning() << "Unable to queue decoded audio:" << SDL_GetError();
  }
#else
  static_cast<void>(opus_sample);
#endif
}
