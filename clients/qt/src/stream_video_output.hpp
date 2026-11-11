#pragma once

#include <memory>
#include <vector>

#include <QImage>
#include <QMutex>
#include <QPoint>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

#include "one/protocol/streaming_session.hpp"
#include "qt_input_mapper.hpp"

class StreamVideoOutput : public QQuickPaintedItem, public one::protocol::VideoRenderer {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool inputEnabled READ inputEnabled NOTIFY inputEnabledChanged)
  Q_PROPERTY(bool inputCaptured READ inputCaptured NOTIFY inputCapturedChanged)

 public:
  explicit StreamVideoOutput(QQuickItem* parent = nullptr);
  ~StreamVideoOutput() override;

  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] bool inputEnabled() const noexcept;
  [[nodiscard]] bool inputCaptured() const noexcept;
  void setInputSession(one::protocol::StreamingSession* session);
  Q_INVOKABLE void releaseInput();
  void paint(QPainter* painter) override;

  [[nodiscard]] one::protocol::VideoRendererCapabilities capabilities() const noexcept override;
  int setup(const one::protocol::VideoStreamInformation& stream) noexcept override;
  void start() noexcept override;
  void stop() noexcept override;
  void cleanup() noexcept override;
  one::protocol::VideoSubmitResult submit(const one::protocol::EncodedVideoFrameView& frame) noexcept override;

 signals:
  void firstFramePresented();
  void inputEnabledChanged();
  void inputCapturedChanged();

 protected:
  void focusOutEvent(QFocusEvent* event) override;
  void hoverMoveEvent(QHoverEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseUngrabEvent() override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  void captureInput();
  void releasePressedInput();
  void handlePointerPosition(const QPoint& globalPosition);
  void sendMouseMove(int deltaX, int deltaY);
  [[nodiscard]] QPoint cursorCenter() const;
  [[nodiscard]] one::protocol::KeyModifiers pressedKeyModifiers() const;

  struct Decoder;
  std::unique_ptr<Decoder> decoder_;
  one::protocol::StreamingSession* input_session_ {nullptr};
  mutable QMutex image_mutex_;
  QImage image_;
  std::vector<QtKeyMapping> pressed_keys_;
  std::vector<one::protocol::MouseButton> pressed_mouse_buttons_;
  QPoint saved_cursor_position_;
  QPoint last_pointer_position_;
  bool presented_first_frame_ {false};
  bool input_captured_ {false};
  bool capture_click_pending_ {false};
  bool restore_cursor_position_ {false};
  bool warp_cursor_ {false};
};
