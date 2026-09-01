#include "ui_environment.hpp"

#include <QCoreApplication>
#include <QSysInfo>

#include <optional>
#include <string_view>

#include "one/ui_mode.hpp"

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QNativeInterface>
#endif

namespace {

#if defined(Q_OS_ANDROID)
bool is_android_television() {
  constexpr int android_ui_mode_type_television = 4;

  const auto context = QNativeInterface::QAndroidApplication::context();
  if (!context.isValid()) {
    return false;
  }

  const auto service_name = QJniObject::fromString(QStringLiteral("uimode"));
  const auto manager = context.callObjectMethod(
      "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;",
      service_name.object<jstring>());

  return manager.isValid() &&
         manager.callMethod<jint>("getCurrentModeType", "()I") == android_ui_mode_type_television;
}
#endif

one::DeviceTraits platform_traits() {
#if defined(Q_OS_TVOS)
  return {one::DeviceClass::television, false, false, true};
#elif defined(Q_OS_ANDROID)
  if (is_android_television()) {
    return {one::DeviceClass::television, false, false, true};
  }
  return {one::DeviceClass::handheld, false, true, false};
#elif defined(Q_OS_IOS)
  return {one::DeviceClass::handheld, false, true, false};
#else
  return {one::DeviceClass::desktop, true, false, true};
#endif
}

std::optional<one::UiMode> parse_ui_mode(const QString& value) {
  if (value.compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0) {
    return one::UiMode::desktop;
  }
  if (value.compare(QStringLiteral("touch"), Qt::CaseInsensitive) == 0) {
    return one::UiMode::touch;
  }
  if (value.compare(QStringLiteral("television"), Qt::CaseInsensitive) == 0 ||
      value.compare(QStringLiteral("tv"), Qt::CaseInsensitive) == 0) {
    return one::UiMode::television;
  }
  return std::nullopt;
}

std::optional<one::UiMode> requested_ui_mode() {
  constexpr std::string_view prefix = "--ui-mode=";
  const auto qt_prefix = QString::fromLatin1(prefix.data(), static_cast<qsizetype>(prefix.size()));

  for (const auto& argument : QCoreApplication::arguments()) {
    if (argument.startsWith(qt_prefix)) {
      return parse_ui_mode(argument.sliced(static_cast<qsizetype>(prefix.size())));
    }
  }

  return parse_ui_mode(qEnvironmentVariable("ONE_UI_MODE"));
}

QString mode_name(const one::UiMode mode) {
  const auto name = one::to_string(mode);
  return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()));
}

}  // namespace

UiEnvironment::UiEnvironment(QObject* parent) : QObject(parent) {
  const auto selected_mode = requested_ui_mode().value_or(one::select_ui_mode(platform_traits()));
  mode_ = mode_name(selected_mode);
  platform_ = QSysInfo::productType();
  television_ = selected_mode == one::UiMode::television;
}

const QString& UiEnvironment::mode() const noexcept {
  return mode_;
}

const QString& UiEnvironment::platform() const noexcept {
  return platform_;
}

bool UiEnvironment::television() const noexcept {
  return television_;
}

