#pragma once

#include <memory>

#include "one/protocol/streaming_session.hpp"

class OpusAudioRenderer final : public one::protocol::AudioRenderer {
 public:
  OpusAudioRenderer();
  ~OpusAudioRenderer() override;

  [[nodiscard]] static bool available() noexcept;
  [[nodiscard]] one::protocol::AudioRendererCapabilities capabilities() const noexcept override;
  int setup(const one::protocol::OpusStreamInformation& stream) noexcept override;
  void start() noexcept override;
  void stop() noexcept override;
  void cleanup() noexcept override;
  void submit(std::span<const std::byte> opusSample) noexcept override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
