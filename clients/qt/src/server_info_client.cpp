#include "server_info_client.hpp"

#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QUuid>

#include "client_identity.hpp"

ServerInfoClient::ServerInfoClient(const ClientIdentity& identity, QObject* parent)
    : QObject(parent), identity_(identity), network_(this) {
  network_.setProxy(QNetworkProxy::NoProxy);
}

void ServerInfoClient::query(const QString& address, const quint16 port) {
  const auto key = requestKey(address, port);
  if (pending_.contains(key)) {
    return;
  }
  pending_.insert(key);

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
  connect(reply, &QNetworkReply::finished, this, [this, reply, address, port, key]() {
    pending_.remove(key);
    if (reply->error() != QNetworkReply::NoError) {
      emit failed(address, port, reply->errorString());
      reply->deleteLater();
      return;
    }

    QString parse_error;
    const auto info = parseServerInfo(reply->readAll(), &parse_error);
    reply->deleteLater();
    if (!info.has_value()) {
      emit failed(address, port, parse_error);
      return;
    }
    emit succeeded(address, port, *info);
  });
}

QString ServerInfoClient::requestKey(const QString& address, const quint16 port) {
  return address + u':' + QString::number(port);
}
