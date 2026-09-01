#include <iostream>

#include "one/protocol/streaming_session.hpp"

namespace {

class TestVideoRenderer final : public one::protocol::VideoRenderer {
 public:
  int setup(const one::protocol::VideoStreamInformation&) noexcept override { return 0; }
  void start() noexcept override {}
  void stop() noexcept override {}
  void cleanup() noexcept override {}
  one::protocol::VideoSubmitResult submit(const one::protocol::EncodedVideoFrameView&) noexcept override {
    return one::protocol::VideoSubmitResult::ok;
  }
};

class TestAudioRenderer final : public one::protocol::AudioRenderer {
 public:
  int setup(const one::protocol::OpusStreamInformation&) noexcept override { return 0; }
  void start() noexcept override {}
  void stop() noexcept override {}
  void cleanup() noexcept override {}
  void submit(std::span<const std::byte>) noexcept override {}
};

class TestListener final : public one::protocol::ConnectionListener {};

}  // namespace

int main() {
  one::protocol::StreamConfiguration configuration;
  TestVideoRenderer video;
  TestAudioRenderer audio;
  TestListener listener;
  one::protocol::StreamingSession session({}, configuration, video, audio, listener);
  if (session.start() != one::protocol::StreamingSession::invalid_server_information || session.active()) {
    std::cerr << "invalid server information should fail without starting a connection\n";
    return 1;
  }
  session.interrupt();
  session.stop();
  return 0;
}
