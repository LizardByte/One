#include "streaming_controller.hpp"

#include <utility>

#include <QMetaObject>
#include <QtConcurrentRun>
#include <QtLogging>

#include "client_identity.hpp"
#include "desktop_gamepad_manager.hpp"
#include "host_settings.hpp"
#include "launch_request.hpp"
#include "opus_audio_renderer.hpp"
#include "stream_video_output.hpp"

namespace {

QString connection_error_message(const int error_code) {
  switch (error_code) {
    case 0:
      return StreamingController::tr("The host ended the stream.");
    case -100:
      return StreamingController::tr("No video was received. Check the host firewall and UDP port 47998.");
    case -101:
      return StreamingController::tr("No complete video frame was received. Try a lower bitrate.");
    case -102:
    case -103:
      return StreamingController::tr("The host stopped streaming unexpectedly. Check for DRM-protected content.");
    case -104:
      return StreamingController::tr("The host reported a video conversion failure.");
    default:
      return StreamingController::tr("The streaming connection ended with error %1.").arg(error_code);
  }
}

}  // namespace

StreamingController::StreamingController(QObject* parent) : QObject(parent) {
  identity_ = std::make_unique<ClientIdentity>();
  gamepad_manager_ = std::make_unique<DesktopGamepadManager>();
  launch_client_ = std::make_unique<LaunchClient>(*identity_);

  connect(
      gamepad_manager_.get(),
      &DesktopGamepadManager::controllerCountChanged,
      this,
      &StreamingController::controllerCountChanged);

  connect(launch_client_.get(), &LaunchClient::succeeded, this, [this](
                                                                         const QString& stable_id,
                                                                         const int application_id,
                                                                         const LaunchResponse& response) {
    if (stable_id == stable_id_ && application_id == pending_application_id_) {
      handleLaunchSucceeded(application_id, response.rtsp_session_url);
    }
  });
  connect(launch_client_.get(), &LaunchClient::failed, this, [this](
                                                                      const QString& stable_id,
                                                                      const int application_id,
                                                                      const QString& launch_error) {
    if (stable_id != stable_id_ || application_id != pending_application_id_) {
      return;
    }
    if (state_ == State::stopping) {
      clearSession();
      setStatus({});
      setState(State::idle);
      return;
    }
    if (state_ != State::launching) {
      return;
    }
    setError(launch_error);
  });
  connect(&start_watcher_, &QFutureWatcher<int>::finished, this, &StreamingController::handleStartFinished);
}

StreamingController::~StreamingController() {
  stop_requested_ = true;
  gamepad_manager_->attachSession(nullptr);
  if (!video_output_.isNull()) {
    video_output_->setInputSession(nullptr);
  }
  if (session_ && start_watcher_.isRunning()) {
    session_->interrupt();
    start_watcher_.waitForFinished();
  }
  if (session_) {
    session_->stop();
  }
}

bool StreamingController::available() const noexcept {
#if defined(ONE_HAS_FFMPEG) && defined(ONE_HAS_PC_MEDIA)
  return OpusAudioRenderer::available();
#else
  return false;
#endif
}

bool StreamingController::busy() const noexcept {
  return state_ != State::idle && state_ != State::error;
}

bool StreamingController::streaming() const noexcept {
  return state_ == State::streaming;
}

int StreamingController::controllerCount() const noexcept {
  return gamepad_manager_->controllerCount();
}

const QString& StreamingController::status() const noexcept {
  return status_;
}

const QString& StreamingController::error() const noexcept {
  return error_;
}

QString StreamingController::unavailableReason() const {
  return available() ? QString {}
                     : tr("This build does not include the desktop FFmpeg, Opus, and SDL3 media backend.");
}

bool StreamingController::startApplication(
    const int application_id,
    const QString& title,
    QObject* video_output) {
  if (!available()) {
    setError(unavailableReason());
    return false;
  }
  if (busy() || stable_id_.isEmpty() || address_.isEmpty() || app_version_.isEmpty() || https_port_ == 0) {
    setError(tr("The selected computer is not ready to start a stream."));
    return false;
  }

  auto* output = qobject_cast<StreamVideoOutput*>(video_output);
  if (output == nullptr || !output->available()) {
    setError(tr("The video output does not have an H.264 decoder."));
    return false;
  }
  if (current_game_id_ != 0 && current_game_id_ != application_id) {
    setError(tr("Another application is already running on %1.").arg(host_name_));
    return false;
  }

  one::protocol::StreamSettings settings;
  settings.video_formats = {
      .h264 = true,
      .hevc = false,
      .hevc_10_bit = false,
      .av1 = false,
      .av1_10_bit = false,
  };
  configuration_ = std::make_unique<one::protocol::StreamConfiguration>(std::move(settings));
  video_output_ = output;
  pending_application_id_ = application_id;
  pending_was_resume_ = current_game_id_ == application_id;
  application_title_ = title;
  stop_requested_ = false;
  pending_termination_error_.reset();
  setError({});
  setStatus(tr("Launching %1…").arg(title));
  setState(State::launching);

  launch_client_->send(
      stable_id_,
      address_,
      https_port_,
      loadServerCertificate(stable_id_),
      pending_was_resume_ ? LaunchAction::resume : LaunchAction::launch,
      configuration_->launch_parameters(),
      {
          .application_id = application_id,
          .gamepad_mask = gamepad_manager_->activeMask(),
      });
  return true;
}

void StreamingController::stop() {
  stop_requested_ = true;
  if (state_ == State::idle) {
    return;
  }
  if (state_ == State::launching) {
    setStatus(tr("Waiting for the launch request to finish…"));
    setState(State::stopping);
    return;
  }
  if (start_result_pending_) {
    setStatus(tr("Stopping connection attempt…"));
    setState(State::stopping);
    if (session_ && start_watcher_.isRunning()) {
      session_->interrupt();
    }
    return;
  }
  if (session_) {
    setState(State::stopping);
    gamepad_manager_->attachSession(nullptr);
    if (!video_output_.isNull()) {
      video_output_->setInputSession(nullptr);
    }
    session_->stop();
  }
  clearSession();
  setStatus({});
  setState(State::idle);
}

void StreamingController::selectHost(
    const QString& stable_id,
    const QString& display_name,
    const QString& address,
    const quint16 https_port,
    const QString& app_version,
    const QString& gfe_version,
    const int codec_mode_support,
    const int current_game_id) {
  stable_id_ = stable_id;
  host_name_ = display_name;
  address_ = address;
  https_port_ = https_port;
  app_version_ = app_version;
  gfe_version_ = gfe_version;
  codec_mode_support_ = codec_mode_support;
  current_game_id_ = current_game_id;
}

void StreamingController::setState(const State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  emit stateChanged();
}

void StreamingController::setStatus(QString status) {
  if (status_ == status) {
    return;
  }
  status_ = std::move(status);
  emit statusChanged();
}

void StreamingController::setError(QString error) {
  if (error_ != error) {
    error_ = std::move(error);
    emit errorChanged();
  }
  if (!error_.isEmpty()) {
    setStatus({});
    setState(State::error);
  }
}

void StreamingController::handleLaunchSucceeded(
    const int application_id,
    const QString& rtsp_session_url) {
  if (state_ == State::stopping || stop_requested_) {
    if (!pending_was_resume_) {
      launch_client_->cancel(stable_id_, address_, https_port_, loadServerCertificate(stable_id_));
    }
    clearSession();
    setStatus({});
    setState(State::idle);
    return;
  }
  if (state_ != State::launching || application_id != pending_application_id_ || video_output_.isNull()) {
    return;
  }

  audio_renderer_ = std::make_unique<OpusAudioRenderer>();
  session_ = std::make_unique<one::protocol::StreamingSession>(
      one::protocol::ServerInformation {
          .address = address_.toStdString(),
          .app_version = app_version_.toStdString(),
          .gfe_version = gfe_version_.toStdString(),
          .rtsp_session_url = rtsp_session_url.toStdString(),
          .codec_mode_support = codec_mode_support_,
      },
      *configuration_,
      *video_output_,
      *audio_renderer_,
      *this);
  current_game_id_ = application_id;
  setStatus(tr("Connecting to %1…").arg(host_name_));
  setState(State::connecting);
  auto* session = session_.get();
  start_result_pending_ = true;
  start_watcher_.setFuture(QtConcurrent::run([session]() { return session->start(); }));
}

void StreamingController::handleStartFinished() {
  const auto result = start_watcher_.result();
  start_result_pending_ = false;
  if (pending_termination_error_.has_value()) {
    const auto termination_error = *pending_termination_error_;
    pending_termination_error_.reset();
    if (session_ && result == 0) {
      session_->stop();
    }
    clearSession();
    setError(connection_error_message(termination_error));
    return;
  }
  if (stop_requested_) {
    if (session_ && result == 0) {
      session_->stop();
    }
    clearSession();
    setStatus({});
    setState(State::idle);
    return;
  }
  if (result != 0) {
    if (!pending_was_resume_) {
      launch_client_->cancel(stable_id_, address_, https_port_, loadServerCertificate(stable_id_));
    }
    clearSession();
    setError(tr("Unable to start the streaming protocol (error %1).").arg(result));
    return;
  }
  if (!video_output_.isNull()) {
    video_output_->setInputSession(session_.get());
  }
  gamepad_manager_->attachSession(session_.get());
  setStatus(tr("Streaming %1").arg(application_title_));
  setState(State::streaming);
}

void StreamingController::clearSession() {
  gamepad_manager_->attachSession(nullptr);
  if (!video_output_.isNull()) {
    video_output_->setInputSession(nullptr);
  }
  session_.reset();
  audio_renderer_.reset();
  configuration_.reset();
  video_output_.clear();
  pending_application_id_ = 0;
  application_title_.clear();
  pending_was_resume_ = false;
}

void StreamingController::postStatus(QString status) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, status = std::move(status)]() mutable { setStatus(std::move(status)); },
      Qt::QueuedConnection);
}

void StreamingController::stage_starting(const std::string_view stage) noexcept {
  postStatus(tr("Connecting: %1…").arg(QString::fromUtf8(stage.data(), static_cast<qsizetype>(stage.size()))));
}

void StreamingController::stage_complete(const std::string_view) noexcept {}

void StreamingController::stage_failed(const std::string_view stage, const int error_code) noexcept {
  postStatus(tr("Connection stage %1 failed (%2).").arg(
      QString::fromUtf8(stage.data(), static_cast<qsizetype>(stage.size()))).arg(error_code));
}

void StreamingController::connection_started() noexcept {
  QMetaObject::invokeMethod(
      this,
      [this]() {
        if (!stop_requested_) {
          setStatus(tr("Streaming %1").arg(application_title_));
          setState(State::streaming);
        }
      },
      Qt::QueuedConnection);
}

void StreamingController::connection_terminated(const int error_code) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, error_code]() {
        if (stop_requested_ || state_ == State::stopping || !session_) {
          return;
        }
        if (start_result_pending_) {
          pending_termination_error_ = error_code;
          if (start_watcher_.isRunning()) {
            session_->interrupt();
          }
          return;
        }
        if (session_) {
          session_->stop();
        }
        clearSession();
        setError(connection_error_message(error_code));
      },
      Qt::QueuedConnection);
}

void StreamingController::log_message(const std::string_view message) noexcept {
  qInfo().noquote() << QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size())).trimmed();
}

void StreamingController::connection_status_changed(const int status) noexcept {
  if (status != 0) {
    postStatus(tr("The network connection is performing poorly."));
  }
}

void StreamingController::hdr_mode_changed(const bool enabled) noexcept {
  if (enabled) {
    postStatus(tr("The host enabled HDR; this software renderer currently displays SDR."));
  }
}

void StreamingController::controller_rumble(
    const std::uint16_t controller_number,
    const std::uint16_t low_frequency_motor,
    const std::uint16_t high_frequency_motor) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, controller_number, low_frequency_motor, high_frequency_motor]() {
        if (session_ && session_->active()) {
          gamepad_manager_->rumble(
              controller_number, low_frequency_motor, high_frequency_motor);
        }
      },
      Qt::QueuedConnection);
}

void StreamingController::controller_trigger_rumble(
    const std::uint16_t controller_number,
    const std::uint16_t left_trigger_motor,
    const std::uint16_t right_trigger_motor) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, controller_number, left_trigger_motor, right_trigger_motor]() {
        if (session_ && session_->active()) {
          gamepad_manager_->rumbleTriggers(
              controller_number, left_trigger_motor, right_trigger_motor);
        }
      },
      Qt::QueuedConnection);
}

void StreamingController::controller_motion_state(
    const std::uint16_t controller_number,
    const one::protocol::ControllerMotionType type,
    const std::uint16_t report_rate_hz) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, controller_number, type, report_rate_hz]() {
        if (session_ && session_->active()) {
          gamepad_manager_->setMotionReporting(controller_number, type, report_rate_hz);
        }
      },
      Qt::QueuedConnection);
}

void StreamingController::controller_led(
    const std::uint16_t controller_number,
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, controller_number, red, green, blue]() {
        if (session_ && session_->active()) {
          gamepad_manager_->setLed(controller_number, red, green, blue);
        }
      },
      Qt::QueuedConnection);
}

void StreamingController::controller_adaptive_triggers(
    const std::uint16_t controller_number,
    const one::protocol::ControllerAdaptiveTriggerEffect& effect) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, controller_number, effect]() {
        if (session_ && session_->active()) {
          gamepad_manager_->setAdaptiveTriggers(controller_number, effect);
        }
      },
      Qt::QueuedConnection);
}
