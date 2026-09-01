#pragma once

#include <memory>
#include <optional>

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QString>

#include "one/protocol/streaming_session.hpp"

class ClientIdentity;
class DesktopGamepadManager;
class LaunchClient;
class OpusAudioRenderer;
class StreamVideoOutput;

class StreamingController final : public QObject, public one::protocol::ConnectionListener {
  Q_OBJECT
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(bool streaming READ streaming NOTIFY stateChanged)
  Q_PROPERTY(int controllerCount READ controllerCount NOTIFY controllerCountChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QString unavailableReason READ unavailableReason CONSTANT)

 public:
  enum class State {
    idle,
    launching,
    connecting,
    streaming,
    stopping,
    error,
  };
  Q_ENUM(State)

  explicit StreamingController(QObject* parent = nullptr);
  ~StreamingController() override;

  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool streaming() const noexcept;
  [[nodiscard]] int controllerCount() const noexcept;
  [[nodiscard]] const QString& status() const noexcept;
  [[nodiscard]] const QString& error() const noexcept;
  [[nodiscard]] QString unavailableReason() const;

  Q_INVOKABLE bool startApplication(int applicationId, const QString& title, QObject* videoOutput);
  Q_INVOKABLE void stop();

 public slots:
  void selectHost(
      const QString& stableId,
      const QString& displayName,
      const QString& address,
      quint16 httpsPort,
      const QString& appVersion,
      const QString& gfeVersion,
      int codecModeSupport,
      int currentGameId);

 signals:
  void stateChanged();
  void statusChanged();
  void errorChanged();
  void controllerCountChanged();

 private:
  void setState(State state);
  void setStatus(QString status);
  void setError(QString error);
  void handleLaunchSucceeded(int applicationId, const QString& rtspSessionUrl);
  void handleStartFinished();
  void clearSession();
  void postStatus(QString status) noexcept;

  void stage_starting(std::string_view stage) noexcept override;
  void stage_complete(std::string_view stage) noexcept override;
  void stage_failed(std::string_view stage, int errorCode) noexcept override;
  void connection_started() noexcept override;
  void connection_terminated(int errorCode) noexcept override;
  void log_message(std::string_view message) noexcept override;
  void connection_status_changed(int status) noexcept override;
  void hdr_mode_changed(bool enabled) noexcept override;
  void controller_rumble(
      std::uint16_t controllerNumber,
      std::uint16_t lowFrequencyMotor,
      std::uint16_t highFrequencyMotor) noexcept override;
  void controller_trigger_rumble(
      std::uint16_t controllerNumber,
      std::uint16_t leftTriggerMotor,
      std::uint16_t rightTriggerMotor) noexcept override;

  std::unique_ptr<ClientIdentity> identity_;
  std::unique_ptr<DesktopGamepadManager> gamepad_manager_;
  std::unique_ptr<LaunchClient> launch_client_;
  std::unique_ptr<one::protocol::StreamConfiguration> configuration_;
  std::unique_ptr<OpusAudioRenderer> audio_renderer_;
  std::unique_ptr<one::protocol::StreamingSession> session_;
  QFutureWatcher<int> start_watcher_;
  QPointer<StreamVideoOutput> video_output_;
  QString stable_id_;
  QString host_name_;
  QString address_;
  QString app_version_;
  QString gfe_version_;
  QString application_title_;
  QString status_;
  QString error_;
  quint16 https_port_ {0};
  int codec_mode_support_ {1};
  int current_game_id_ {0};
  int pending_application_id_ {0};
  State state_ {State::idle};
  bool pending_was_resume_ {false};
  bool stop_requested_ {false};
  bool start_result_pending_ {false};
  std::optional<int> pending_termination_error_;
};
