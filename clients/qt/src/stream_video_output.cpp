#include "stream_video_output.hpp"

#include <algorithm>
#include <cstring>
#include <thread>

#include <QByteArray>
#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPainter>
#include <QRectF>
#include <QtLogging>

#ifdef ONE_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
#endif

namespace {

#ifdef ONE_HAS_FFMPEG
AVCodecID codec_id(const one::protocol::VideoFormat format) noexcept {
  using one::protocol::VideoFormat;
  switch (format) {
    case VideoFormat::h264:
    case VideoFormat::h264_444:
      return AV_CODEC_ID_H264;
    case VideoFormat::hevc:
    case VideoFormat::hevc_10_bit:
    case VideoFormat::hevc_444:
    case VideoFormat::hevc_10_bit_444:
      return AV_CODEC_ID_HEVC;
    case VideoFormat::av1:
    case VideoFormat::av1_10_bit:
    case VideoFormat::av1_444:
    case VideoFormat::av1_10_bit_444:
      return AV_CODEC_ID_AV1;
    case VideoFormat::unknown:
      return AV_CODEC_ID_NONE;
  }
  return AV_CODEC_ID_NONE;
}

QString ffmpeg_error(const int result) {
  char message[AV_ERROR_MAX_STRING_SIZE] {};
  return av_make_error_string(message, sizeof(message), result);
}
#endif

}  // namespace

struct StreamVideoOutput::Decoder {
#ifdef ONE_HAS_FFMPEG
  AVCodecContext* context {nullptr};
  AVFrame* frame {nullptr};
  SwsContext* scaler {nullptr};
  QByteArray packet_data;
#endif
};

StreamVideoOutput::StreamVideoOutput(QQuickItem* parent)
    : QQuickPaintedItem(parent), decoder_(std::make_unique<Decoder>()) {
  setAntialiasing(false);
  setOpaquePainting(true);
}

StreamVideoOutput::~StreamVideoOutput() {
  cleanup();
}

bool StreamVideoOutput::available() const noexcept {
#ifdef ONE_HAS_FFMPEG
  return avcodec_find_decoder(AV_CODEC_ID_H264) != nullptr;
#else
  return false;
#endif
}

void StreamVideoOutput::paint(QPainter* painter) {
  painter->fillRect(boundingRect(), Qt::black);

  QImage image;
  {
    const QMutexLocker lock(&image_mutex_);
    image = image_;
  }
  if (image.isNull()) {
    return;
  }

  auto target = QRectF(QPointF(0, 0), image.size().scaled(size().toSize(), Qt::KeepAspectRatio));
  target.moveCenter(boundingRect().center());
  painter->drawImage(target, image);
}

one::protocol::VideoRendererCapabilities StreamVideoOutput::capabilities() const noexcept {
  const auto processor_count = std::max(1U, std::thread::hardware_concurrency());
  return {
      .reference_frame_invalidation_hevc = true,
      .reference_frame_invalidation_av1 = true,
      .slices_per_frame = static_cast<std::uint8_t>(std::min(4U, processor_count)),
  };
}

int StreamVideoOutput::setup(const one::protocol::VideoStreamInformation& stream) noexcept {
  cleanup();
#ifdef ONE_HAS_FFMPEG
  const auto id = codec_id(stream.format);
  const auto* codec = id == AV_CODEC_ID_NONE ? nullptr : avcodec_find_decoder(id);
  if (codec == nullptr) {
    qWarning() << "No FFmpeg decoder is available for negotiated video format" << static_cast<int>(stream.format);
    return -1;
  }

  decoder_->context = avcodec_alloc_context3(codec);
  decoder_->frame = av_frame_alloc();
  if (decoder_->context == nullptr || decoder_->frame == nullptr) {
    qWarning() << "Unable to allocate FFmpeg decoder state";
    cleanup();
    return -1;
  }

  decoder_->context->width = stream.width;
  decoder_->context->height = stream.height;
  decoder_->context->flags |= AV_CODEC_FLAG_LOW_DELAY;
  decoder_->context->flags2 |= AV_CODEC_FLAG2_FAST;
  decoder_->context->thread_count = 0;
  decoder_->context->thread_type = FF_THREAD_SLICE;
  const auto result = avcodec_open2(decoder_->context, codec, nullptr);
  if (result < 0) {
    qWarning() << "Unable to open FFmpeg decoder:" << ffmpeg_error(result);
    cleanup();
    return result;
  }

  {
    const QMutexLocker lock(&image_mutex_);
    image_ = {};
    presented_first_frame_ = false;
  }
  qInfo() << "FFmpeg decoder initialized:" << codec->name << stream.width << 'x' << stream.height;
  return 0;
#else
  static_cast<void>(stream);
  return -1;
#endif
}

void StreamVideoOutput::start() noexcept {}

void StreamVideoOutput::stop() noexcept {}

void StreamVideoOutput::cleanup() noexcept {
#ifdef ONE_HAS_FFMPEG
  if (decoder_) {
    sws_freeContext(decoder_->scaler);
    decoder_->scaler = nullptr;
    av_frame_free(&decoder_->frame);
    avcodec_free_context(&decoder_->context);
    decoder_->packet_data.clear();
  }
#endif
}

one::protocol::VideoSubmitResult StreamVideoOutput::submit(
    const one::protocol::EncodedVideoFrameView& encoded_frame) noexcept {
#ifdef ONE_HAS_FFMPEG
  if (decoder_->context == nullptr || decoder_->frame == nullptr || encoded_frame.data_size() == 0) {
    return one::protocol::VideoSubmitResult::request_key_frame;
  }

  decoder_->packet_data.resize(
      static_cast<qsizetype>(encoded_frame.data_size()) + AV_INPUT_BUFFER_PADDING_SIZE);
  auto offset = qsizetype {0};
  for (std::size_t index = 0; index < encoded_frame.buffer_count(); ++index) {
    const auto buffer = encoded_frame.buffer(index);
    std::memcpy(
        decoder_->packet_data.data() + offset,
        buffer.bytes.data(),
        buffer.bytes.size());
    offset += static_cast<qsizetype>(buffer.bytes.size());
  }
  std::memset(decoder_->packet_data.data() + offset, 0, AV_INPUT_BUFFER_PADDING_SIZE);

  AVPacket packet {};
  packet.data = reinterpret_cast<std::uint8_t*>(decoder_->packet_data.data());
  packet.size = static_cast<int>(offset);
  packet.pts = static_cast<std::int64_t>(encoded_frame.presentation_time_us());
  packet.dts = packet.pts;
  packet.flags = encoded_frame.key_frame() ? AV_PKT_FLAG_KEY : 0;

  auto result = avcodec_send_packet(decoder_->context, &packet);
  if (result < 0) {
    qWarning() << "FFmpeg rejected a video packet:" << ffmpeg_error(result);
    return one::protocol::VideoSubmitResult::request_key_frame;
  }

  while ((result = avcodec_receive_frame(decoder_->context, decoder_->frame)) >= 0) {
    const auto width = decoder_->frame->width;
    const auto height = decoder_->frame->height;
    if (width <= 0 || height <= 0) {
      continue;
    }

    decoder_->scaler = sws_getCachedContext(
        decoder_->scaler,
        width,
        height,
        static_cast<AVPixelFormat>(decoder_->frame->format),
        width,
        height,
        AV_PIX_FMT_RGBA,
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (decoder_->scaler == nullptr) {
      qWarning() << "Unable to create FFmpeg color converter";
      return one::protocol::VideoSubmitResult::request_key_frame;
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    std::uint8_t* destination[] {image.bits(), nullptr, nullptr, nullptr};
    const int destination_lines[] {static_cast<int>(image.bytesPerLine()), 0, 0, 0};
    if (sws_scale(
            decoder_->scaler,
            decoder_->frame->data,
            decoder_->frame->linesize,
            0,
            height,
            destination,
            destination_lines) <= 0) {
      qWarning() << "FFmpeg failed to convert a decoded frame";
      return one::protocol::VideoSubmitResult::request_key_frame;
    }

    bool first_frame = false;
    {
      const QMutexLocker lock(&image_mutex_);
      image_ = std::move(image);
      if (!presented_first_frame_) {
        presented_first_frame_ = true;
        first_frame = true;
      }
    }
    QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
    if (first_frame) {
      QMetaObject::invokeMethod(this, &StreamVideoOutput::firstFramePresented, Qt::QueuedConnection);
    }
    av_frame_unref(decoder_->frame);
  }

  if (result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
    qWarning() << "FFmpeg failed while decoding video:" << ffmpeg_error(result);
    return one::protocol::VideoSubmitResult::request_key_frame;
  }
  return one::protocol::VideoSubmitResult::ok;
#else
  static_cast<void>(encoded_frame);
  return one::protocol::VideoSubmitResult::request_key_frame;
#endif
}
