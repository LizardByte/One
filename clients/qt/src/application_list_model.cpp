#include "application_list_model.hpp"

#include <algorithm>

#include "application_list_client.hpp"
#include "client_identity.hpp"
#include "host_settings.hpp"

ApplicationListModel::ApplicationListModel(QObject* parent) : QAbstractListModel(parent) {
  identity_ = std::make_unique<ClientIdentity>();
  client_ = std::make_unique<ApplicationListClient>(*identity_);

  connect(client_.get(), &ApplicationListClient::succeeded, this, [this](
                                                                         const QString& stable_id,
                                                                         QVector<Application> applications) {
    if (stable_id != stable_id_) {
      return;
    }
    std::sort(applications.begin(), applications.end(), [](const Application& left, const Application& right) {
      return QString::localeAwareCompare(left.title, right.title) < 0;
    });
    replaceApplications(std::move(applications));
    setError({});
    setLoading(false);
  });
  connect(client_.get(), &ApplicationListClient::failed, this, [this](
                                                                      const QString& stable_id,
                                                                      const QString& error) {
    if (stable_id != stable_id_) {
      return;
    }
    setError(error);
    setLoading(false);
  });
}

ApplicationListModel::~ApplicationListModel() = default;

int ApplicationListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : applications_.size();
}

QVariant ApplicationListModel::data(const QModelIndex& index, const int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }

  const auto& application = applications_[index.row()];
  switch (role) {
    case ApplicationIdRole:
      return application.id;
    case TitleRole:
      return application.title;
    case HdrSupportedRole:
      return application.hdr_supported;
    case AppCollectorGameRole:
      return application.app_collector_game;
    default:
      return {};
  }
}

QHash<int, QByteArray> ApplicationListModel::roleNames() const {
  return {
      {ApplicationIdRole, "applicationId"},
      {TitleRole, "title"},
      {HdrSupportedRole, "hdrSupported"},
      {AppCollectorGameRole, "appCollectorGame"},
  };
}

bool ApplicationListModel::loading() const noexcept {
  return loading_;
}

const QString& ApplicationListModel::error() const noexcept {
  return error_;
}

const QString& ApplicationListModel::hostName() const noexcept {
  return host_name_;
}

void ApplicationListModel::loadHost(
    const QString& stable_id,
    const QString& display_name,
    const QString& address,
    const quint16 https_port,
    const QString& app_version,
    const QString& gfe_version,
    const int codec_mode_support,
    const int current_game_id) {
  const auto host_changed = stable_id_ != stable_id;
  stable_id_ = stable_id;
  address_ = address;
  https_port_ = https_port;
  app_version_ = app_version;
  gfe_version_ = gfe_version;
  codec_mode_support_ = codec_mode_support;
  current_game_id_ = current_game_id;
  if (host_name_ != display_name) {
    host_name_ = display_name;
    emit hostNameChanged();
  }
  if (host_changed) {
    replaceApplications({});
  }
  refresh();
}

void ApplicationListModel::refresh() {
  if (stable_id_.isEmpty() || address_.isEmpty() || https_port_ == 0 || loading_) {
    return;
  }
  setError({});
  setLoading(true);
  client_->query(stable_id_, address_, https_port_, loadServerCertificate(stable_id_));
}

void ApplicationListModel::setLoading(const bool loading) {
  if (loading_ == loading) {
    return;
  }
  loading_ = loading;
  emit loadingChanged();
}

void ApplicationListModel::setError(QString error) {
  if (error_ == error) {
    return;
  }
  error_ = std::move(error);
  emit errorChanged();
}

void ApplicationListModel::replaceApplications(QVector<Application> applications) {
  beginResetModel();
  applications_ = std::move(applications);
  endResetModel();
  emit countChanged();
}
