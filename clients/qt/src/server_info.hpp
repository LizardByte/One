#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

struct ServerInfo {
  QString stable_id;
  QString display_name;
  QString app_version;
  QString gfe_version;
  quint16 https_port {47984};
  int codec_mode_support {1};
  bool paired {false};
};

[[nodiscard]] std::optional<ServerInfo> parseServerInfo(const QByteArray& xml, QString* error = nullptr);
