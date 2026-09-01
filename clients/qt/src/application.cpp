#include "application.hpp"

#include <utility>

#include <QXmlStreamReader>

std::optional<QVector<Application>> parseApplicationList(const QByteArray& xml, QString* error) {
  struct ParsedApplication {
    Application application;
    bool title_seen {false};
    bool id_seen {false};
  };

  QXmlStreamReader reader(xml);
  QVector<ParsedApplication> parsed_applications;
  int current_application = -1;
  int status_code = 0;
  QString status_message;

  while (!reader.atEnd()) {
    const auto token = reader.readNext();
    if (token == QXmlStreamReader::StartElement) {
      if (reader.name() == u"root") {
        status_code = static_cast<int>(reader.attributes().value(QStringLiteral("status_code")).toUInt());
        status_message = reader.attributes().value(QStringLiteral("status_message")).toString();
      } else if (reader.name() == u"App") {
        parsed_applications.append(ParsedApplication {});
        current_application = parsed_applications.size() - 1;
      } else if (current_application >= 0) {
        auto& application = parsed_applications[current_application];
        if (reader.name() == u"AppTitle") {
          application.application.title = reader.readElementText();
          if (application.application.title.isNull()) {
            application.application.title = QStringLiteral("");
          }
          application.title_seen = true;
        } else if (reader.name() == u"ID") {
          bool valid = false;
          application.application.id = reader.readElementText().toInt(&valid);
          application.id_seen = valid;
        } else if (reader.name() == u"IsHdrSupported") {
          application.application.hdr_supported = reader.readElementText() == QStringLiteral("1");
        } else if (reader.name() == u"IsAppCollectorGame") {
          application.application.app_collector_game = reader.readElementText() == QStringLiteral("1");
        }
      }
    } else if (token == QXmlStreamReader::EndElement && reader.name() == u"App") {
      current_application = -1;
    }
  }

  auto fail = [error](const QString& message) -> std::optional<QVector<Application>> {
    if (error != nullptr) {
      *error = message;
    }
    return std::nullopt;
  };

  if (reader.hasError()) {
    return fail(reader.errorString());
  }
  if (status_code != 200) {
    return fail(status_message.isEmpty() ? QStringLiteral("Server returned status %1").arg(status_code)
                                         : status_message);
  }

  QVector<Application> applications;
  applications.reserve(parsed_applications.size());
  for (auto& parsed : parsed_applications) {
    if (!parsed.title_seen || !parsed.id_seen || parsed.application.id < 0) {
      return fail(QStringLiteral("Application list contains an incomplete entry"));
    }
    applications.append(std::move(parsed.application));
  }
  return applications;
}
