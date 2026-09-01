#include "host_settings.hpp"

#include <QSettings>
#include <QUrl>

namespace {

QString server_certificate_key(const QString& stable_id) {
  return QStringLiteral("hosts/%1/serverCertificate")
      .arg(QString::fromLatin1(QUrl::toPercentEncoding(stable_id)));
}

}  // namespace

QByteArray loadServerCertificate(const QString& stable_id) {
  return QSettings().value(server_certificate_key(stable_id)).toByteArray();
}

void saveServerCertificate(const QString& stable_id, const QByteArray& certificate) {
  QSettings().setValue(server_certificate_key(stable_id), certificate);
}
