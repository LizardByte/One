#pragma once

#include <QByteArray>
#include <QString>

[[nodiscard]] QByteArray loadServerCertificate(const QString& stableId);
void saveServerCertificate(const QString& stableId, const QByteArray& certificate);
[[nodiscard]] bool hostNeedsPairing(const QString& stableId);
void setHostNeedsPairing(const QString& stableId, bool required);
[[nodiscard]] QString hostInstanceId(const QString& serverId, quint16 httpPort);
void migrateLegacyHostCertificate(const QString& serverId, quint16 httpPort);
