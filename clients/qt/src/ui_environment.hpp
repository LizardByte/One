#pragma once

#include <QObject>
#include <QString>

class UiEnvironment final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString mode READ mode CONSTANT FINAL)
  Q_PROPERTY(QString platform READ platform CONSTANT FINAL)
  Q_PROPERTY(bool television READ television CONSTANT FINAL)

 public:
  explicit UiEnvironment(QObject* parent = nullptr);

  [[nodiscard]] const QString& mode() const noexcept;
  [[nodiscard]] const QString& platform() const noexcept;
  [[nodiscard]] bool television() const noexcept;

 private:
  QString mode_;
  QString platform_;
  bool television_ {false};
};

