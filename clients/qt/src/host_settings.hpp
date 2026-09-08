#pragma once

#include <QByteArray>
#include <QString>

#include "one/host.hpp"

[[nodiscard]] std::vector<one::Host> loadKnownHosts();
void saveKnownHosts(const std::vector<one::Host>& hosts);

[[nodiscard]] QByteArray loadServerCertificate(const QString& stableId);
void saveServerCertificate(const QString& stableId, const QByteArray& certificate);
[[nodiscard]] bool hostNeedsPairing(const QString& stableId);
void setHostNeedsPairing(const QString& stableId, bool required);
[[nodiscard]] QString hostInstanceId(const QString& serverId, quint16 httpPort);
void migrateLegacyHostCertificate(const QString& serverId, quint16 httpPort);
