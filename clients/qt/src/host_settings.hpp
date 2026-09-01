#pragma once

#include <QByteArray>
#include <QString>

[[nodiscard]] QByteArray loadServerCertificate(const QString& stableId);
void saveServerCertificate(const QString& stableId, const QByteArray& certificate);
