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
  if (session.send_mouse_move(1, -1) != one::protocol::StreamingSession::input_not_ready ||
      session.send_mouse_button(one::protocol::MouseButton::left, one::protocol::ButtonAction::press) !=
          one::protocol::StreamingSession::input_not_ready ||
      session.send_vertical_scroll(120) != one::protocol::StreamingSession::input_not_ready ||
      session.send_horizontal_scroll(-120) != one::protocol::StreamingSession::input_not_ready ||
      session.send_key(0x41, one::protocol::KeyAction::press, {}) !=
          one::protocol::StreamingSession::input_not_ready ||
      session.send_utf8_text("One") != one::protocol::StreamingSession::input_not_ready) {
    std::cerr << "input should be rejected while the connection is inactive\n";
    return 1;
  }
  session.interrupt();
  session.stop();
  return 0;
}
