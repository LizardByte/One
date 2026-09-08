#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>

#include "application.hpp"

class ClientIdentity;

class ApplicationListClient final : public QObject {
  Q_OBJECT

 public:
  explicit ApplicationListClient(const ClientIdentity& identity, QObject* parent = nullptr);

  void query(const QString& stableId, const QString& address, quint16 httpsPort, const QByteArray& serverCertificate,
             int applicationId = -1);

 signals:
  void succeeded(const QString& stableId, const QVector<Application>& applications);
  void failed(const QString& stableId, const QString& error);
  void authenticationRequired(const QString& stableId);
  void artworkReady(const QString& stableId, int applicationId, const QString& imageSource, const QString& error = {});

 private:
  const ClientIdentity& identity_;
  QNetworkAccessManager network_;
  QSet<QString> pending_;
};
