#include "server_info.hpp"

#include <QHash>
#include <QXmlStreamReader>

std::optional<ServerInfo> parseServerInfo(const QByteArray& xml, QString* error, int* response_status) {
  if (response_status) *response_status = 0;
  QXmlStreamReader reader(xml);
  QHash<QString, QString> values;
  int status_code = 0;
  QString status_message;

  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement) {
      continue;
    }

    if (reader.name() == u"root") {
      status_code = reader.attributes().value(QStringLiteral("status_code")).toInt();
      status_message = reader.attributes().value(QStringLiteral("status_message")).toString();
      continue;
    }

    const auto name = reader.name().toString();
    values.insert(name, reader.readElementText());
  }

  auto fail = [error](const QString& message) -> std::optional<ServerInfo> {
    if (error != nullptr) {
      *error = message;
    }
    return std::nullopt;
  };

  if (reader.hasError()) {
    return fail(reader.errorString());
  }
  if (response_status) *response_status = status_code;
  if (status_code != 200) {
    return fail(status_message.isEmpty() ? QStringLiteral("Server returned status %1").arg(status_code)
                                         : status_message);
  }
  if (values.value(QStringLiteral("uniqueid")).isEmpty()) {
    return fail(QStringLiteral("Server information is missing its unique ID"));
  }

  ServerInfo info;
  info.stable_id = values.value(QStringLiteral("uniqueid"));
  info.display_name = values.value(QStringLiteral("hostname"));
  info.app_version = values.value(QStringLiteral("appversion"));
  info.gfe_version = values.value(QStringLiteral("GfeVersion"));
  info.state = values.value(QStringLiteral("state"));
  info.paired = values.value(QStringLiteral("PairStatus")) == QStringLiteral("1");
  info.pairing_status_known = info.paired || values.value(QStringLiteral("PairStatus")) == QStringLiteral("0");

  // Older NVIDIA hosts can retain currentgame after the session has ended, so
  // it is only meaningful while the server explicitly reports itself busy.
  if (info.state.endsWith(QStringLiteral("_SERVER_BUSY"))) {
    bool game_valid = false;
    const auto current_game = values.value(QStringLiteral("currentgame")).toInt(&game_valid);
    if (game_valid && current_game > 0) {
      info.current_game_id = current_game;
    }
  }

  bool port_valid = false;
  const auto https_port = values.value(QStringLiteral("HttpsPort")).toUShort(&port_valid);
  if (port_valid && https_port != 0) {
    info.https_port = https_port;
  }

  bool codec_valid = false;
  const auto codec_support = values.value(QStringLiteral("ServerCodecModeSupport")).toInt(&codec_valid);
  if (codec_valid) {
    info.codec_mode_support = codec_support;
  }
  return info;
}
