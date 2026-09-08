#include "launch_request.hpp"

#include <QDebug>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QUuid>
#include <QXmlStreamReader>

#include "client_identity.hpp"

namespace {

QString bool_number(const bool value) { return value ? QStringLiteral("1") : QStringLiteral("0"); }

QString request_key(const QString& stable_id, const int application_id) {
  return stable_id + u':' + QString::number(application_id);
}

void append_moonlight_parameters(QUrlQuery& query, QString parameters) {
  while (parameters.startsWith(u'&') || parameters.startsWith(u'?')) {
    parameters.remove(0, 1);
  }
  const QUrlQuery moonlight_query(parameters);
  for (const auto& [name, value] : moonlight_query.queryItems()) {
    query.addQueryItem(name, value);
  }
}

}  // namespace

QUrlQuery buildLaunchQuery(const one::protocol::StreamLaunchParameters& stream, const LaunchRequestOptions& options) {
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("appid"), QString::number(options.application_id));

  const auto requested_fps = stream.frames_per_second > 60 && options.is_nvidia_server ? 0 : stream.frames_per_second;
  query.addQueryItem(QStringLiteral("mode"),
                     QStringLiteral("%1x%2x%3").arg(stream.width).arg(stream.height).arg(requested_fps));
  query.addQueryItem(QStringLiteral("additionalStates"), QStringLiteral("1"));
  query.addQueryItem(QStringLiteral("sops"), bool_number(options.optimize_game_settings));
  query.addQueryItem(QStringLiteral("rikey"), QString::fromStdString(stream.remote_input_aes_key_hex));
  query.addQueryItem(QStringLiteral("rikeyid"), QString::number(stream.remote_input_aes_key_id));

  if (stream.hdr_requested) {
    query.addQueryItem(QStringLiteral("hdrMode"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("clientHdrCapVersion"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("clientHdrCapSupportedFlagsInUint32"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("clientHdrCapMetaDataId"), QStringLiteral("NV_STATIC_METADATA_TYPE_1"));
    query.addQueryItem(QStringLiteral("clientHdrCapDisplayData"), QStringLiteral("0x0x0x0x0x0x0x0x0x0"));
  }

  query.addQueryItem(QStringLiteral("localAudioPlayMode"), bool_number(options.play_audio_on_host));
  query.addQueryItem(QStringLiteral("surroundAudioInfo"), QString::number(stream.surround_audio_info));
  query.addQueryItem(QStringLiteral("remoteControllersBitmap"), QString::number(options.gamepad_mask));
  query.addQueryItem(QStringLiteral("gcmap"), QString::number(options.gamepad_mask));
  query.addQueryItem(QStringLiteral("gcpersist"), bool_number(options.persist_gamepads));
  append_moonlight_parameters(query, QString::fromStdString(stream.moonlight_query_parameters));
  return query;
}

std::optional<LaunchResponse> parseLaunchResponse(const QByteArray& xml, QString* error, int* responseStatus) {
  if (responseStatus != nullptr) *responseStatus = 0;
  QXmlStreamReader reader(xml);
  int status_code = 0;
  QString status_message;
  LaunchResponse response;

  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement) {
      continue;
    }
    if (reader.name() == u"root") {
      status_code = reader.attributes().value(QStringLiteral("status_code")).toInt();
      status_message = reader.attributes().value(QStringLiteral("status_message")).toString();
    } else if (reader.name() == u"sessionUrl0") {
      response.rtsp_session_url = reader.readElementText();
    }
  }

  auto fail = [error](const QString& message) -> std::optional<LaunchResponse> {
    if (error != nullptr) {
      *error = message;
    }
    return std::nullopt;
  };
  if (reader.hasError()) {
    return fail(reader.errorString());
  }
  if (responseStatus != nullptr) *responseStatus = status_code;
  if (status_code != 200) {
    return fail(status_message.isEmpty() ? QStringLiteral("Server returned status %1").arg(status_code)
                                         : status_message);
  }
  return response;
}

LaunchClient::LaunchClient(const ClientIdentity& identity, QObject* parent)
    : QObject(parent), identity_(identity), network_(this) {
  network_.setProxy(QNetworkProxy::NoProxy);
}

void LaunchClient::send(const QString& stable_id, const QString& address, const quint16 https_port,
                        const QByteArray& server_certificate_pem, const LaunchAction action,
                        const one::protocol::StreamLaunchParameters& stream, const LaunchRequestOptions& options) {
  const auto key = request_key(stable_id, options.application_id);
  if (stable_id.isEmpty() || address.isEmpty() || https_port == 0 || options.application_id <= 0 ||
      pending_.contains(key)) {
    return;
  }

  const QSslCertificate server_certificate(server_certificate_pem);
  if (server_certificate.isNull()) {
    emit authenticationRequired(stable_id);
    emit failed(stable_id, options.application_id, tr("The paired server certificate is missing or invalid."));
    return;
  }

  QUrl url;
  url.setScheme(QStringLiteral("https"));
  url.setHost(address);
  url.setPort(https_port);
  url.setPath(action == LaunchAction::resume ? QStringLiteral("/resume") : QStringLiteral("/launch"));

  auto query = buildLaunchQuery(stream, options);
  query.addQueryItem(QStringLiteral("uniqueid"), identity_.uniqueId());
  query.addQueryItem(QStringLiteral("uuid"), QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setTransferTimeout(10000);
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
  request.setSslConfiguration(identity_.sslConfiguration());

  pending_.insert(key);
  auto* reply = network_.get(request);
  connect(reply, &QNetworkReply::sslErrors, reply, [reply, server_certificate](const QList<QSslError>& errors) {
    for (const auto& ssl_error : errors) {
      if (ssl_error.certificate() != server_certificate) {
        if (!ssl_error.certificate().isNull()) reply->setProperty("oneCertificateMismatch", true);
        return;
      }
    }
    reply->ignoreSslErrors(errors);
  });

  connect(reply, &QNetworkReply::finished, this, [this, reply, stable_id, server_certificate, options, key]() {
    pending_.remove(key);
    const auto peer_certificate = reply->sslConfiguration().peerCertificate();
    if (reply->property("oneCertificateMismatch").toBool() ||
        (!peer_certificate.isNull() && peer_certificate != server_certificate)) {
      emit authenticationRequired(stable_id);
      reply->deleteLater();
      emit failed(stable_id, options.application_id,
                  tr("The server certificate no longer matches this Sunshine instance. Pair it again to continue."));
      return;
    }
    if (reply->error() != QNetworkReply::NoError) {
      if (peer_certificate == server_certificate &&
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401)
        emit authenticationRequired(stable_id);
      const auto network_error = reply->errorString();
      reply->deleteLater();
      emit failed(stable_id, options.application_id, network_error);
      return;
    }
    if (reply->sslConfiguration().peerCertificate() != server_certificate) {
      emit authenticationRequired(stable_id);
      reply->deleteLater();
      emit failed(stable_id, options.application_id,
                  tr("The server certificate no longer matches the paired computer."));
      return;
    }

    QString parse_error;
    int response_status = 0;
    const auto response = parseLaunchResponse(reply->readAll(), &parse_error, &response_status);
    reply->deleteLater();
    if (!response.has_value()) {
      if (response_status == 401) emit authenticationRequired(stable_id);
      emit failed(stable_id, options.application_id, parse_error);
      return;
    }
    emit succeeded(stable_id, options.application_id, *response);
  });
}

void LaunchClient::cancel(const QString& stable_id, const QString& address, const quint16 https_port,
                          const QByteArray& server_certificate_pem) {
  const QSslCertificate server_certificate(server_certificate_pem);
  if (stable_id.isEmpty() || address.isEmpty() || https_port == 0 || server_certificate.isNull()) {
    return;
  }

  QUrl url;
  url.setScheme(QStringLiteral("https"));
  url.setHost(address);
  url.setPort(https_port);
  url.setPath(QStringLiteral("/cancel"));
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
  connect(reply, &QNetworkReply::sslErrors, reply, [reply, server_certificate](const QList<QSslError>& errors) {
    for (const auto& ssl_error : errors) {
      if (ssl_error.certificate() != server_certificate) {
        if (!ssl_error.certificate().isNull()) reply->setProperty("oneCertificateMismatch", true);
        return;
      }
    }
    reply->ignoreSslErrors(errors);
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, stable_id, server_certificate]() {
    const auto peer_certificate = reply->sslConfiguration().peerCertificate();
    if (reply->property("oneCertificateMismatch").toBool() ||
        (!peer_certificate.isNull() && peer_certificate != server_certificate)) {
      emit authenticationRequired(stable_id);
    } else if (peer_certificate == server_certificate) {
      int response_status = 0;
      if (reply->error() == QNetworkReply::NoError)
        static_cast<void>(parseLaunchResponse(reply->readAll(), nullptr, &response_status));
      if (response_status == 401 || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401)
        emit authenticationRequired(stable_id);
    }
    if (reply->error() != QNetworkReply::NoError || reply->sslConfiguration().peerCertificate() != server_certificate) {
      qWarning() << "Unable to cancel the host application:" << reply->errorString();
    }
    reply->deleteLater();
  });
}
