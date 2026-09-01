#pragma once

#include <optional>

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

struct Application {
  int id {-1};
  QString title;
  bool hdr_supported {false};
  bool app_collector_game {false};

  bool operator==(const Application&) const = default;
};

[[nodiscard]] std::optional<QVector<Application>> parseApplicationList(
    const QByteArray& xml,
    QString* error = nullptr);

Q_DECLARE_METATYPE(Application)
Q_DECLARE_METATYPE(QVector<Application>)
