#include "server_info.hpp"

#include <QHash>
#include <QXmlStreamReader>

std::optional<ServerInfo> parseServerInfo(const QByteArray& xml, QString* error) {
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
  if (status_code != 200) {
    return fail(status_message.isEmpty() ? QStringLiteral("Server returned status %1").arg(status_code) : status_message);
  }
  if (values.value(QStringLiteral("uniqueid")).isEmpty()) {
    return fail(QStringLiteral("Server information is missing its unique ID"));
  }

  ServerInfo info;
  info.stable_id = values.value(QStringLiteral("uniqueid"));
  info.display_name = values.value(QStringLiteral("hostname"));
  info.app_version = values.value(QStringLiteral("appversion"));
  info.gfe_version = values.value(QStringLiteral("GfeVersion"));
  info.paired = values.value(QStringLiteral("PairStatus")) == QStringLiteral("1");

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
