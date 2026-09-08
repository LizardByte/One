#include "application_list_model.hpp"

#include <algorithm>

#include "application_list_client.hpp"
#include "client_identity.hpp"
#include "host_settings.hpp"

ApplicationListModel::ApplicationListModel(QObject* parent) : QAbstractListModel(parent) {
  identity_ = std::make_unique<ClientIdentity>();
  client_ = std::make_unique<ApplicationListClient>(*identity_);
  connect(client_.get(), &ApplicationListClient::artworkReady, this,
          [this](const QString& stable_id, const int application_id, const QString& image_source) {
            if (stable_id != stable_id_ || pairing_required_ || image_source.isEmpty()) return;
            for (int row = 0; row < applications_.size(); ++row) {
              if (applications_[row].id == application_id) {
                artwork_.insert(application_id, image_source);
                emit dataChanged(index(row), index(row), {ArtworkRole});
                return;
              }
            }
          });
  connect(client_.get(), &ApplicationListClient::authenticationRequired, this, [this](const QString& stable_id) {
    emit hostAuthenticationFailed(stable_id);
    requirePairing(stable_id);
  });

  connect(client_.get(), &ApplicationListClient::succeeded, this,
          [this](const QString& stable_id, QVector<Application> applications) {
            if (stable_id != stable_id_ || pairing_required_) {
              return;
            }
            std::sort(applications.begin(), applications.end(), [](const Application& left, const Application& right) {
              return QString::localeAwareCompare(left.title, right.title) < 0;
            });
            replaceApplications(std::move(applications));
            setError({});
            setLoading(false);
          });
  connect(client_.get(), &ApplicationListClient::failed, this, [this](const QString& stable_id, const QString& error) {
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
    case ArtworkRole:
      return artwork_.value(application.id);
    default:
      return {};
  }
}

QHash<int, QByteArray> ApplicationListModel::roleNames() const {
  return {
      {ApplicationIdRole, "applicationId"},       {TitleRole, "title"},     {HdrSupportedRole, "hdrSupported"},
      {AppCollectorGameRole, "appCollectorGame"}, {ArtworkRole, "artwork"},
  };
}

bool ApplicationListModel::loading() const noexcept { return loading_; }

const QString& ApplicationListModel::error() const noexcept { return error_; }

const QString& ApplicationListModel::hostName() const noexcept { return host_name_; }

bool ApplicationListModel::pairingRequired() const noexcept { return pairing_required_; }

void ApplicationListModel::requirePairing(const QString& stable_id) {
  if (stable_id != stable_id_) return;
  setPairingRequired(true);
  replaceApplications({});
  setError(tr("Pair this Sunshine instance again to continue."));
  setLoading(false);
}

void ApplicationListModel::setPairingRequired(const bool required) {
  if (pairing_required_ == required) return;
  pairing_required_ = required;
  emit pairingRequiredChanged();
}

void ApplicationListModel::loadHost(const QString& stable_id, const QString& display_name, const QString& address,
                                    const quint16 https_port, const QString& app_version, const QString& gfe_version,
                                    const int codec_mode_support, const int current_game_id) {
  const auto host_changed = stable_id_ != stable_id;
  setPairingRequired(false);
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
    // An outstanding request for a different instance must not block this one.
    setLoading(false);
  }
  refresh();
}

void ApplicationListModel::refresh() {
  if (stable_id_.isEmpty() || address_.isEmpty() || https_port_ == 0 || loading_ || pairing_required_) {
    return;
  }
  setError({});
  setLoading(true);
  client_->query(stable_id_, address_, https_port_, loadServerCertificate(stable_id_));
}

void ApplicationListModel::requestArtwork(const int application_id) {
  if (pairing_required_ || stable_id_.isEmpty() || artwork_requested_.contains(application_id)) return;
  const auto app = std::find_if(applications_.begin(), applications_.end(),
                                [application_id](const Application& value) { return value.id == application_id; });
  if (app == applications_.end()) return;
  artwork_requested_.insert(application_id);
  client_->query(stable_id_, address_, https_port_, loadServerCertificate(stable_id_), application_id);
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
  artwork_.clear();
  artwork_requested_.clear();
  applications_ = std::move(applications);
  endResetModel();
  emit countChanged();
}
