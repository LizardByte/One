#pragma once

#include <optional>

#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrlQuery>

#include "one/protocol/stream_configuration.hpp"

class ClientIdentity;

enum class LaunchAction {
  launch,
  resume,
};

struct LaunchRequestOptions {
  int application_id {0};
  bool is_nvidia_server {false};
  bool optimize_game_settings {true};
  bool play_audio_on_host {false};
  int gamepad_mask {0};
  bool persist_gamepads {true};
};

struct LaunchResponse {
  QString rtsp_session_url;
};

[[nodiscard]] QUrlQuery buildLaunchQuery(
    const one::protocol::StreamLaunchParameters& stream,
    const LaunchRequestOptions& options);
[[nodiscard]] std::optional<LaunchResponse> parseLaunchResponse(const QByteArray& xml, QString* error = nullptr);

class LaunchClient final : public QObject {
  Q_OBJECT

 public:
  explicit LaunchClient(const ClientIdentity& identity, QObject* parent = nullptr);

  void send(
      const QString& stableId,
      const QString& address,
      quint16 httpsPort,
      const QByteArray& serverCertificate,
      LaunchAction action,
      const one::protocol::StreamLaunchParameters& stream,
      const LaunchRequestOptions& options);
  void cancel(
      const QString& stableId,
      const QString& address,
      quint16 httpsPort,
      const QByteArray& serverCertificate);

 signals:
  void succeeded(const QString& stableId, int applicationId, const LaunchResponse& response);
  void failed(const QString& stableId, int applicationId, const QString& error);

 private:
  const ClientIdentity& identity_;
  QNetworkAccessManager network_;
  QSet<QString> pending_;
};
