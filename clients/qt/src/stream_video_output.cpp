#include "stream_video_output.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <thread>

#include <QByteArray>
#include <QCursor>
#include <QDebug>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QQuickWindow>
#include <QRectF>
#include <QWheelEvent>
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

std::optional<one::protocol::MouseButton> mouse_button(const Qt::MouseButton button) noexcept {
  using one::protocol::MouseButton;
  switch (button) {
    case Qt::LeftButton:
      return MouseButton::left;
    case Qt::MiddleButton:
      return MouseButton::middle;
    case Qt::RightButton:
      return MouseButton::right;
    case Qt::BackButton:
      return MouseButton::extra_1;
    case Qt::ForwardButton:
      return MouseButton::extra_2;
    default:
      return std::nullopt;
  }
}

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
  setAcceptedMouseButtons(Qt::AllButtons);
  setAcceptHoverEvents(true);
  setFlag(ItemIsFocusScope, true);
  setKeepMouseGrab(true);
}

StreamVideoOutput::~StreamVideoOutput() {
  setInputSession(nullptr);
  cleanup();
}

bool StreamVideoOutput::available() const noexcept {
#ifdef ONE_HAS_FFMPEG
  return avcodec_find_decoder(AV_CODEC_ID_H264) != nullptr;
#else
  return false;
#endif
}

bool StreamVideoOutput::inputEnabled() const noexcept {
  return input_session_ != nullptr && input_session_->active();
}

bool StreamVideoOutput::inputCaptured() const noexcept {
  return input_captured_;
}

void StreamVideoOutput::setInputSession(one::protocol::StreamingSession* session) {
  if (input_session_ == session) {
    return;
  }
  releaseInput();
  input_session_ = session;
  emit inputEnabledChanged();
}

void StreamVideoOutput::releaseInput() {
  capture_click_pending_ = false;
  releasePressedInput();
  if (!input_captured_) {
    return;
  }

  input_captured_ = false;
  setKeepMouseGrab(false);
  ungrabMouse();
  unsetCursor();
  if (restore_cursor_position_) {
    QCursor::setPos(saved_cursor_position_);
  }
  restore_cursor_position_ = false;
  emit inputCapturedChanged();
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

void StreamVideoOutput::captureInput() {
  if (input_captured_ || !inputEnabled()) {
    return;
  }
  input_captured_ = true;
  saved_cursor_position_ = QCursor::pos();
  restore_cursor_position_ = true;
  last_pointer_position_ = saved_cursor_position_;
  warp_cursor_ = !QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
  setCursor(Qt::BlankCursor);
  setKeepMouseGrab(true);
  grabMouse();
  forceActiveFocus(Qt::MouseFocusReason);
  if (warp_cursor_) {
    QCursor::setPos(cursorCenter());
  }
  emit inputCapturedChanged();
}

void StreamVideoOutput::releasePressedInput() {
  if (input_session_ != nullptr && input_session_->active()) {
    while (!pressed_mouse_buttons_.empty()) {
      const auto button = pressed_mouse_buttons_.back();
      pressed_mouse_buttons_.pop_back();
      static_cast<void>(input_session_->send_mouse_button(button, one::protocol::ButtonAction::release));
    }
    while (!pressed_keys_.empty()) {
      const auto key = pressed_keys_.back();
      pressed_keys_.pop_back();
      static_cast<void>(input_session_->send_key(
          key.virtual_key,
          one::protocol::KeyAction::release,
          pressedKeyModifiers(),
          key.normalized));
    }
  } else {
    pressed_mouse_buttons_.clear();
    pressed_keys_.clear();
  }
}

void StreamVideoOutput::handlePointerPosition(const QPoint& global_position) {
  if (!input_captured_ || !inputEnabled()) {
    return;
  }
  if (warp_cursor_) {
    const auto center = cursorCenter();
    const auto delta = global_position - center;
    if (!delta.isNull()) {
      sendMouseMove(delta.x(), delta.y());
      QCursor::setPos(center);
    }
  } else {
    const auto delta = global_position - last_pointer_position_;
    last_pointer_position_ = global_position;
    if (!delta.isNull()) {
      sendMouseMove(delta.x(), delta.y());
    }
  }
}

void StreamVideoOutput::sendMouseMove(int delta_x, int delta_y) {
  constexpr auto minimum = static_cast<int>(std::numeric_limits<std::int16_t>::min());
  constexpr auto maximum = static_cast<int>(std::numeric_limits<std::int16_t>::max());
  while (delta_x != 0 || delta_y != 0) {
    const auto x = std::clamp(delta_x, minimum, maximum);
    const auto y = std::clamp(delta_y, minimum, maximum);
    static_cast<void>(input_session_->send_mouse_move(
        static_cast<std::int16_t>(x),
        static_cast<std::int16_t>(y)));
    delta_x -= x;
    delta_y -= y;
  }
}

QPoint StreamVideoOutput::cursorCenter() const {
  if (window() == nullptr) {
    return QCursor::pos();
  }
  return window()->mapToGlobal(mapToScene(boundingRect().center()).toPoint());
}

one::protocol::KeyModifiers StreamVideoOutput::pressedKeyModifiers() const {
  std::vector<std::uint16_t> virtual_keys;
  virtual_keys.reserve(pressed_keys_.size());
  for (const auto& key : pressed_keys_) {
    virtual_keys.push_back(key.virtual_key);
  }
  return keyModifiersForPressedKeys(virtual_keys);
}

void StreamVideoOutput::focusOutEvent(QFocusEvent* event) {
  releaseInput();
  QQuickPaintedItem::focusOutEvent(event);
}

void StreamVideoOutput::hoverMoveEvent(QHoverEvent* event) {
  handlePointerPosition(event->globalPosition().toPoint());
  event->accept();
}

void StreamVideoOutput::keyPressEvent(QKeyEvent* event) {
  if (!input_captured_ || event->isAutoRepeat()) {
    event->setAccepted(input_captured_);
    return;
  }
  const auto mapping = mapQtKey(
      event->key(),
      event->modifiers(),
      event->nativeVirtualKey(),
      event->nativeScanCode());
  if (!mapping.has_value()) {
    event->ignore();
    return;
  }
  if (isReleaseCaptureShortcut(mapping->virtual_key, pressedKeyModifiers())) {
    releaseInput();
    event->accept();
    return;
  }
  if (std::ranges::find_if(pressed_keys_, [mapping](const auto& key) {
        return key.virtual_key == mapping->virtual_key;
      }) != pressed_keys_.end()) {
    event->accept();
    return;
  }

  pressed_keys_.push_back(*mapping);
  static_cast<void>(input_session_->send_key(
      mapping->virtual_key,
      one::protocol::KeyAction::press,
      pressedKeyModifiers(),
      mapping->normalized));
  event->accept();
}

void StreamVideoOutput::keyReleaseEvent(QKeyEvent* event) {
  if (!input_captured_ || event->isAutoRepeat()) {
    event->setAccepted(input_captured_);
    return;
  }
  const auto mapping = mapQtKey(
      event->key(),
      event->modifiers(),
      event->nativeVirtualKey(),
      event->nativeScanCode());
  if (!mapping.has_value()) {
    event->ignore();
    return;
  }
  const auto pressed = std::ranges::find_if(pressed_keys_, [mapping](const auto& key) {
    return key.virtual_key == mapping->virtual_key;
  });
  if (pressed == pressed_keys_.end()) {
    event->accept();
    return;
  }
  const auto normalized = pressed->normalized;
  pressed_keys_.erase(pressed);
  static_cast<void>(input_session_->send_key(
      mapping->virtual_key,
      one::protocol::KeyAction::release,
      pressedKeyModifiers(),
      normalized));
  event->accept();
}

void StreamVideoOutput::mouseMoveEvent(QMouseEvent* event) {
  handlePointerPosition(event->globalPosition().toPoint());
  event->accept();
}

void StreamVideoOutput::mousePressEvent(QMouseEvent* event) {
  if (!input_captured_) {
    capture_click_pending_ = event->button() == Qt::LeftButton && inputEnabled();
    event->accept();
    return;
  }
  const auto button = mouse_button(event->button());
  if (button.has_value() &&
      std::ranges::find(pressed_mouse_buttons_, *button) == pressed_mouse_buttons_.end()) {
    pressed_mouse_buttons_.push_back(*button);
    static_cast<void>(input_session_->send_mouse_button(*button, one::protocol::ButtonAction::press));
  }
  event->accept();
}

void StreamVideoOutput::mouseReleaseEvent(QMouseEvent* event) {
  if (!input_captured_) {
    if (capture_click_pending_ && event->button() == Qt::LeftButton) {
      capture_click_pending_ = false;
      captureInput();
    }
    event->accept();
    return;
  }
  const auto button = mouse_button(event->button());
  if (button.has_value()) {
    const auto pressed = std::ranges::find(pressed_mouse_buttons_, *button);
    if (pressed != pressed_mouse_buttons_.end()) {
      pressed_mouse_buttons_.erase(pressed);
      static_cast<void>(input_session_->send_mouse_button(*button, one::protocol::ButtonAction::release));
    }
  }
  event->accept();
}

void StreamVideoOutput::mouseUngrabEvent() {
  if (input_captured_) {
    releaseInput();
  }
}

void StreamVideoOutput::wheelEvent(QWheelEvent* event) {
  if (!input_captured_ || !inputEnabled()) {
    event->ignore();
    return;
  }
  const auto delta = event->angleDelta();
  if (delta.y() != 0) {
    static_cast<void>(input_session_->send_vertical_scroll(
        static_cast<std::int16_t>(std::clamp(
            delta.y(),
            static_cast<int>(std::numeric_limits<std::int16_t>::min()),
            static_cast<int>(std::numeric_limits<std::int16_t>::max())))));
  }
  if (delta.x() != 0) {
    static_cast<void>(input_session_->send_horizontal_scroll(
        static_cast<std::int16_t>(std::clamp(
            delta.x(),
            static_cast<int>(std::numeric_limits<std::int16_t>::min()),
            static_cast<int>(std::numeric_limits<std::int16_t>::max())))));
  }
  event->accept();
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
