#include "server_info_client.hpp"

#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QUrlQuery>
#include <QUuid>

#include "client_identity.hpp"
#include "host_settings.hpp"

ServerInfoClient::ServerInfoClient(const ClientIdentity& identity, QObject* parent)
    : QObject(parent), identity_(identity), network_(this) {
  network_.setProxy(QNetworkProxy::NoProxy);
}

void ServerInfoClient::query(const QString& address, const quint16 port) {
  const auto key = requestKey(address, port);
  if (pending_.contains(key)) {
    return;
  }

  QUrl url;
  url.setScheme(QStringLiteral("http"));
  url.setHost(address);
  url.setPort(port);
  url.setPath(QStringLiteral("/serverinfo"));

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("uniqueid"), identity_.uniqueId());
  query.addQueryItem(QStringLiteral("uuid"), QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setTransferTimeout(5000);
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);

  auto* reply = network_.get(request);
  pending_.insert(key, reply);
  connect(reply, &QNetworkReply::finished, this, [this, reply, address, port, key]() {
    if (pending_.value(key) != reply) {
      reply->deleteLater();
      return;
    }
    if (reply->error() != QNetworkReply::NoError) {
      pending_.remove(key);
      emit failed(address, port, reply->errorString());
      reply->deleteLater();
      return;
    }

    QString parse_error;
    const auto info = parseServerInfo(reply->readAll(), &parse_error);
    reply->deleteLater();
    if (!info.has_value()) {
      pending_.remove(key);
      emit failed(address, port, parse_error);
      return;
    }
    verifyPairing(address, port, *info);
  });
}

void ServerInfoClient::cancel(const QString& address, const quint16 port) {
  if (auto* reply = pending_.take(requestKey(address, port))) reply->abort();
}

void ServerInfoClient::verifyPairing(const QString& address, const quint16 port, const ServerInfo& info) {
  const auto key = requestKey(address, port);
  const auto instance_id = hostInstanceId(info.stable_id, port);
  migrateLegacyHostCertificate(info.stable_id, port);
  const auto certificate_pem = loadServerCertificate(instance_id);
  if (certificate_pem.isEmpty()) {
    pending_.remove(key);
    emit succeeded(address, port, info);
    return;
  }
  const QSslCertificate certificate(certificate_pem);
  if (certificate.isNull()) {
    pending_.remove(key);
    emit pairingStatus(instance_id, false);
    emit succeeded(address, port, info);
    return;
  }

  QUrl url;
  url.setScheme(QStringLiteral("https"));
  url.setHost(address);
  url.setPort(info.https_port);
  url.setPath(QStringLiteral("/serverinfo"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("uniqueid"), identity_.uniqueId());
  query.addQueryItem(QStringLiteral("uuid"), QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(5000);
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
  request.setSslConfiguration(identity_.sslConfiguration());
  auto* reply = network_.get(request);
  pending_.insert(key, reply);
  connect(reply, &QNetworkReply::sslErrors, reply, [reply, certificate](const QList<QSslError>& errors) {
    for (const auto& error : errors) {
      if (error.certificate() != certificate) {
        if (!error.certificate().isNull()) reply->setProperty("oneCertificateMismatch", true);
        return;
      }
    }
    reply->ignoreSslErrors(errors);
  });
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, address, port, key, info, instance_id, certificate, certificate_pem]() {
            if (pending_.value(key) != reply) {
              reply->deleteLater();
              return;
            }
            pending_.remove(key);
            auto result = info;
            // A poll started before a new certificate was paired must not revoke it.
            if (loadServerCertificate(instance_id) == certificate_pem) {
              const auto peer = reply->sslConfiguration().peerCertificate();
              if (reply->property("oneCertificateMismatch").toBool() || (!peer.isNull() && peer != certificate)) {
                emit pairingStatus(instance_id, false);
              } else if (peer == certificate) {
                const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status == 401 || status == 403) {
                  emit pairingStatus(instance_id, false);
                } else if (reply->error() == QNetworkReply::NoError) {
                  int response_status = 0;
                  const auto authenticated = parseServerInfo(reply->readAll(), nullptr, &response_status);
                  if (response_status == 401 || response_status == 403) {
                    emit pairingStatus(instance_id, false);
                  } else if (authenticated && authenticated->stable_id == info.stable_id) {
                    result = *authenticated;
                    if (authenticated->pairing_status_known) emit pairingStatus(instance_id, authenticated->paired);
                  }
                }
              }
            }
            // Plain HTTP still established reachability. Transient TLS/HTTP errors
            // neither revoke trust nor falsely report a reachable host as offline.
            reply->deleteLater();
            emit succeeded(address, port, result);
          });
}

QString ServerInfoClient::requestKey(const QString& address, const quint16 port) {
  return address + u':' + QString::number(port);
}
