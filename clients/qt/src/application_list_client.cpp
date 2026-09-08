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

void ApplicationListClient::query(const QString& stable_id, const QString& address, const quint16 https_port,
                                  const QByteArray& server_certificate_pem, const int application_id) {
  const bool artwork = application_id >= 0;
  const auto pending_key = artwork ? stable_id + QStringLiteral("/art/%1").arg(application_id) : stable_id;
  const auto report_failure = [this, stable_id, application_id, artwork](const QString& error) {
    if (artwork)
      emit artworkReady(stable_id, application_id, {});
    else
      emit failed(stable_id, error);
  };
  if (stable_id.isEmpty() || address.isEmpty() || https_port == 0 || pending_.contains(pending_key)) {
    return;
  }

  const QSslCertificate server_certificate(server_certificate_pem);
  if (server_certificate.isNull()) {
    emit authenticationRequired(stable_id);
    report_failure(tr("The paired server certificate is missing or invalid."));
    return;
  }

  QUrl url;
  url.setScheme(QStringLiteral("https"));
  url.setHost(address);
  url.setPort(https_port);
  url.setPath(artwork ? QStringLiteral("/appasset") : QStringLiteral("/applist"));

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("uniqueid"), identity_.uniqueId());
  query.addQueryItem(QStringLiteral("uuid"), QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
  if (artwork) {
    query.addQueryItem(QStringLiteral("appid"), QString::number(application_id));
    query.addQueryItem(QStringLiteral("AssetType"), QStringLiteral("2"));
    query.addQueryItem(QStringLiteral("AssetIdx"), QStringLiteral("0"));
  }
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setTransferTimeout(5000);
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
  request.setSslConfiguration(identity_.sslConfiguration());

  pending_.insert(pending_key);
  auto* reply = network_.get(request);
  if (artwork) {
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
      if (received > 4 * 1024 * 1024) reply->abort();
    });
  }
  connect(reply, &QNetworkReply::sslErrors, reply, [reply, server_certificate](const QList<QSslError>& errors) {
    for (const auto& error : errors) {
      if (error.certificate() != server_certificate) {
        if (!error.certificate().isNull()) reply->setProperty("oneCertificateMismatch", true);
        return;
      }
    }
    reply->ignoreSslErrors(errors);
  });

  connect(reply, &QNetworkReply::finished, this,
          [this, reply, stable_id, server_certificate, pending_key, artwork, application_id, report_failure]() {
            pending_.remove(pending_key);
            const auto peer_certificate = reply->sslConfiguration().peerCertificate();
            if (reply->property("oneCertificateMismatch").toBool() ||
                (!peer_certificate.isNull() && peer_certificate != server_certificate)) {
              emit authenticationRequired(stable_id);
              reply->deleteLater();
              report_failure(
                  tr("The server certificate no longer matches this Sunshine instance. Pair it again to continue."));
              return;
            }
            if (reply->error() != QNetworkReply::NoError) {
              // An HTTP rejection is meaningful only after verifying the paired server.
              if (reply->sslConfiguration().peerCertificate() == server_certificate &&
                  reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
                emit authenticationRequired(stable_id);
              }
              const auto error = reply->errorString();
              reply->deleteLater();
              report_failure(error);
              return;
            }
            if (reply->sslConfiguration().peerCertificate() != server_certificate) {
              emit authenticationRequired(stable_id);
              reply->deleteLater();
              report_failure(tr("The server certificate no longer matches the paired computer."));
              return;
            }

            const auto payload = reply->readAll();
            if (artwork) {
              reply->deleteLater();
              if (payload.size() <= 4 * 1024 * 1024 && payload.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) {
                emit artworkReady(stable_id, application_id,
                                  QStringLiteral("data:image/png;base64,") + QString::fromLatin1(payload.toBase64()));
              } else {
                int status = 0;
                static_cast<void>(parseApplicationList(payload, nullptr, &status));
                if (status == 401) emit authenticationRequired(stable_id);
                emit artworkReady(stable_id, application_id, {});
              }
              return;
            }
            QString parse_error;
            int response_status = 0;
            const auto applications = parseApplicationList(payload, &parse_error, &response_status);
            reply->deleteLater();
            if (!applications.has_value()) {
              if (response_status == 401) emit authenticationRequired(stable_id);
              report_failure(parse_error);
              return;
            }
            emit succeeded(stable_id, *applications);
          });
}
