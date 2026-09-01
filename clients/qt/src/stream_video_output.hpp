#pragma once

#include <memory>

#include <QImage>
#include <QMutex>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

#include "one/protocol/streaming_session.hpp"

class StreamVideoOutput : public QQuickPaintedItem, public one::protocol::VideoRenderer {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(bool available READ available CONSTANT)

 public:
  explicit StreamVideoOutput(QQuickItem* parent = nullptr);
  ~StreamVideoOutput() override;

  [[nodiscard]] bool available() const noexcept;
  void paint(QPainter* painter) override;

  [[nodiscard]] one::protocol::VideoRendererCapabilities capabilities() const noexcept override;
  int setup(const one::protocol::VideoStreamInformation& stream) noexcept override;
  void start() noexcept override;
  void stop() noexcept override;
  void cleanup() noexcept override;
  one::protocol::VideoSubmitResult submit(const one::protocol::EncodedVideoFrameView& frame) noexcept override;

 signals:
  void firstFramePresented();

 private:
  struct Decoder;
  std::unique_ptr<Decoder> decoder_;
  mutable QMutex image_mutex_;
  QImage image_;
  bool presented_first_frame_ {false};
};
