#include "host_settings.hpp"

#include <QSettings>
#include <QUrl>

namespace {

QString host_settings_key(const QString& stable_id, const QString& setting) {
  return QStringLiteral("hosts/%1/%2").arg(QString::fromLatin1(QUrl::toPercentEncoding(stable_id)), setting);
}

}  // namespace

QByteArray loadServerCertificate(const QString& stable_id) {
  return QSettings().value(host_settings_key(stable_id, QStringLiteral("serverCertificate"))).toByteArray();
}

void saveServerCertificate(const QString& stable_id, const QByteArray& certificate) {
  QSettings().setValue(host_settings_key(stable_id, QStringLiteral("serverCertificate")), certificate);
  setHostNeedsPairing(stable_id, false);
}

bool hostNeedsPairing(const QString& stable_id) {
  return QSettings().value(host_settings_key(stable_id, QStringLiteral("needsPairing")), false).toBool();
}

void setHostNeedsPairing(const QString& stable_id, const bool required) {
  QSettings().setValue(host_settings_key(stable_id, QStringLiteral("needsPairing")), required);
}

QString hostInstanceId(const QString& server_id, const quint16 http_port) {
  return QStringLiteral("%1@%2").arg(QString::fromLatin1(QUrl::toPercentEncoding(server_id))).arg(http_port);
}

void migrateLegacyHostCertificate(const QString& server_id, const quint16 http_port) {
  // Old builds did not scope trust by port. Only the original default-port
  // instance may inherit that trust; another instance must pair independently.
  if (http_port != 47989) return;
  const auto instance_id = hostInstanceId(server_id, http_port);
  QSettings settings;
  if (settings.contains(host_settings_key(instance_id, QStringLiteral("serverCertificate")))) return;
  const auto certificate = loadServerCertificate(server_id);
  if (certificate.isEmpty()) return;
  const auto needs_pairing = hostNeedsPairing(server_id);
  saveServerCertificate(instance_id, certificate);
  setHostNeedsPairing(instance_id, needs_pairing);
}
