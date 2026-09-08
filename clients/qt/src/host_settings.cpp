#include "host_settings.hpp"

#include <QSettings>
#include <QUrl>
#include <QVariantMap>

namespace {

QString host_settings_key(const QString& stable_id, const QString& setting) {
  return QStringLiteral("hosts/%1/%2").arg(QString::fromLatin1(QUrl::toPercentEncoding(stable_id)), setting);
}

}  // namespace

std::vector<one::Host> loadKnownHosts() {
  std::vector<one::Host> hosts;
  const auto entries = QSettings().value(QStringLiteral("knownHosts")).toList();
  for (const auto& entry : entries) {
    const auto values = entry.toMap();
    const auto address = values.value(QStringLiteral("address")).toString().trimmed();
    const auto port = values.value(QStringLiteral("port")).toInt();
    if (address.isEmpty() || port <= 0 || port > 65535) continue;
    const auto id = values.value(QStringLiteral("id")).toString();
    hosts.push_back({
        .stable_id = id.toStdString(),
        .display_name = values.value(QStringLiteral("name"), address).toString().toStdString(),
        .endpoint = {address.toStdString(), static_cast<quint16>(port)},
        .reachability = one::HostReachability::unknown,
        .pairing = !id.isEmpty() && !hostNeedsPairing(id) && !loadServerCertificate(id).isEmpty()
                       ? one::HostPairingState::paired
                       : one::HostPairingState::unpaired,
        .manually_added = values.value(QStringLiteral("manual")).toBool(),
    });
  }
  return hosts;
}

void saveKnownHosts(const std::vector<one::Host>& hosts) {
  QVariantList entries;
  for (const auto& host : hosts) {
    entries.append(QVariantMap{
        {QStringLiteral("id"), QString::fromStdString(host.stable_id)},
        {QStringLiteral("name"), QString::fromStdString(host.display_name)},
        {QStringLiteral("address"), QString::fromStdString(host.endpoint.address)},
        {QStringLiteral("port"), host.endpoint.port},
        {QStringLiteral("manual"), host.manually_added},
    });
  }
  QSettings settings;
  // Reachability and in-progress pairing are deliberately not persisted.
  // Avoid rewriting unchanged settings on each reachability poll.
  if (settings.value(QStringLiteral("knownHosts")).toList() != entries)
    settings.setValue(QStringLiteral("knownHosts"), entries);
}

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
