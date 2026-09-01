#include "application_list_client.hpp"

#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QUrlQuery>
#include <QUuid>

#include "client_identity.hpp"

ApplicationListClient::ApplicationListClient(const ClientIdentity& identity, QObject* parent)
    : QObject(parent), identity_(identity), network_(this) {
  network_.setProxy(QNetworkProxy::NoProxy);
}

void ApplicationListClient::query(
    const QString& stable_id,
    const QString& address,
    const quint16 https_port,
    const QByteArray& server_certificate_pem) {
  if (stable_id.isEmpty() || address.isEmpty() || https_port == 0 || pending_.contains(stable_id)) {
    return;
  }

  const QSslCertificate server_certificate(server_certificate_pem);
  if (server_certificate.isNull()) {
    emit failed(stable_id, tr("The paired server certificate is missing or invalid."));
    return;
  }

  QUrl url;
  url.setScheme(QStringLiteral("https"));
  url.setHost(address);
  url.setPort(https_port);
  url.setPath(QStringLiteral("/applist"));

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("uniqueid"), identity_.uniqueId());
  query.addQueryItem(QStringLiteral("uuid"), QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setTransferTimeout(5000);
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
  request.setSslConfiguration(identity_.sslConfiguration());

  pending_.insert(stable_id);
  auto* reply = network_.get(request);
  connect(reply, &QNetworkReply::sslErrors, reply, [reply, server_certificate](const QList<QSslError>& errors) {
    for (const auto& error : errors) {
      if (error.certificate() != server_certificate) {
        return;
      }
    }
    reply->ignoreSslErrors(errors);
  });

  connect(reply, &QNetworkReply::finished, this, [this, reply, stable_id, server_certificate]() {
    pending_.remove(stable_id);
    if (reply->error() != QNetworkReply::NoError) {
      const auto error = reply->errorString();
      reply->deleteLater();
      emit failed(stable_id, error);
      return;
    }
    if (reply->sslConfiguration().peerCertificate() != server_certificate) {
      reply->deleteLater();
      emit failed(stable_id, tr("The server certificate no longer matches the paired computer."));
      return;
    }

    QString parse_error;
    const auto applications = parseApplicationList(reply->readAll(), &parse_error);
    reply->deleteLater();
    if (!applications.has_value()) {
      emit failed(stable_id, parse_error);
      return;
    }
    emit succeeded(stable_id, *applications);
  });
}
