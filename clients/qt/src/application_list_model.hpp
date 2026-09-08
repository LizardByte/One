#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <memory>

#include "application.hpp"

class ApplicationListModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QString hostName READ hostName NOTIFY hostNameChanged)
  Q_PROPERTY(bool pairingRequired READ pairingRequired NOTIFY pairingRequiredChanged)

 public:
  enum Role {
    ApplicationIdRole = Qt::UserRole + 1,
    TitleRole,
    HdrSupportedRole,
    AppCollectorGameRole,
    ArtworkRole,
  };
  Q_ENUM(Role)

  explicit ApplicationListModel(QObject* parent = nullptr);
  ~ApplicationListModel() override;

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] const QString& error() const noexcept;
  [[nodiscard]] const QString& hostName() const noexcept;
  [[nodiscard]] bool pairingRequired() const noexcept;

 public slots:
  void loadHost(const QString& stableId, const QString& displayName, const QString& address, quint16 httpsPort,
                const QString& appVersion, const QString& gfeVersion, int codecModeSupport, int currentGameId);
  void refresh();
  void requestArtwork(int applicationId);
  void requirePairing(const QString& stableId);

 signals:
  void countChanged();
  void loadingChanged();
  void errorChanged();
  void hostNameChanged();
  void pairingRequiredChanged();
  void hostAuthenticationFailed(const QString& stableId);

 private:
  void setLoading(bool loading);
  void setError(QString error);
  void replaceApplications(QVector<Application> applications);
  void setPairingRequired(bool required);

  std::unique_ptr<class ClientIdentity> identity_;
  std::unique_ptr<class ApplicationListClient> client_;
  QVector<Application> applications_;
  QHash<int, QString> artwork_;
  QSet<int> artwork_requested_;
  QString stable_id_;
  QString host_name_;
  QString address_;
  QString app_version_;
  QString gfe_version_;
  QString error_;
  quint16 https_port_{0};
  int codec_mode_support_{1};
  int current_game_id_{0};
  bool loading_{false};
  bool pairing_required_{false};
};
