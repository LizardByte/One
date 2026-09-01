#include <iostream>

#include <QGuiApplication>

#include "opus_audio_renderer.hpp"
#include "stream_video_output.hpp"

int main(int argc, char* argv[]) {
  QGuiApplication application(argc, argv);
  StreamVideoOutput video;
  if (!video.available()) {
    std::cerr << "decoder-enabled FFmpeg must provide an H.264 decoder\n";
    return 1;
  }
  if (!OpusAudioRenderer::available()) {
    std::cerr << "desktop media build must provide Opus audio\n";
    return 1;
  }
  if (video.setup({
          .format = one::protocol::VideoFormat::h264,
          .width = 1280,
          .height = 720,
          .redraw_rate = 60,
      }) != 0) {
    std::cerr << "FFmpeg H.264 decoder should initialize\n";
    return 1;
  }
  const auto capabilities = video.capabilities();
  if (capabilities.slices_per_frame == 0 || !capabilities.reference_frame_invalidation_hevc ||
      !capabilities.reference_frame_invalidation_av1) {
    std::cerr << "software decoder capabilities should be advertised\n";
    return 1;
  }
  video.cleanup();
  return 0;
}
