#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>

namespace {

bool expect(const bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

QQuickItem* find_item(QQuickItem* root, const QString& name) {
  if (!root->isVisible()) return nullptr;
  if (root->objectName() == name) return root;
  for (auto* child : root->childItems()) {
    if (auto* item = find_item(child, name)) return item;
  }
  return nullptr;
}

std::array<QRectF, 3> navigation_bounds(QQuickWindow* window) {
  std::array<QRectF, 3> bounds;
  for (int index = 0; index < 3; ++index) {
    if (auto* item = find_item(window->contentItem(), "navigation" + QString::number(index)))
      bounds[index] = item->mapRectToScene(item->boundingRect());
  }
  return bounds;
}

bool expect_navigation(QQuickWindow* window, const std::array<QRectF, 3>& original) {
  const auto current = navigation_bounds(window);
  for (int index = 0; index < 3; ++index) {
    if (!expect(!current[index].isEmpty() && qAbs(current[index].x() - original[index].x()) < 0.5 &&
                    qAbs(current[index].y() - original[index].y()) < 0.5 &&
                    qAbs(current[index].width() - original[index].width()) < 0.5 &&
                    qAbs(current[index].height() - original[index].height()) < 0.5,
                "Navigation must not move or resize when changing pages")) {
      std::cerr << "navigation" << index << " expected " << original[index].x() << ',' << original[index].y() << ' '
                << original[index].width() << 'x' << original[index].height() << " but got " << current[index].x()
                << ',' << current[index].y() << ' ' << current[index].width() << 'x' << current[index].height() << '\n';
      return false;
    }
  }
  return true;
}

bool click(QQuickWindow* window, const char* name) {
  auto* item = find_item(window->contentItem(), QString::fromLatin1(name));
  if (!expect(item && item->isVisible() && item->isEnabled(), name)) return false;
  const auto position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
  if (!expect(window->contentItem()->contains(position), "Control must be inside the window")) return false;
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, position);
  QTest::qWait(300);
  return true;
}

bool capture(QQuickWindow* window, const QString& directory, const QString& name) {
  if (directory.isEmpty()) return true;
  QTest::qWait(100);
  return expect(window->grabWindow().save(directory + '/' + name + QStringLiteral(".png")), "Screenshot failed");
}

QImage capture_avatar(QQuickWindow* window, QQuickItem* avatar) {
  if (!expect(avatar != nullptr, "The host avatar must exist")) return {};
  const auto grab = avatar->grabToImage(QSize(qRound(avatar->width() * window->devicePixelRatio()),
                                              qRound(avatar->height() * window->devicePixelRatio())));
  if (!expect(!grab.isNull(), "The host avatar must render")) return {};
  QSignalSpy ready(grab.data(), &QQuickItemGrabResult::ready);
  if (grab->image().isNull() && !expect(ready.wait(2000), "The host avatar capture must complete")) return {};
  return grab->image();
}

bool expect_even_offline_ring(QQuickWindow* window, QQuickItem* avatar) {
  // Check rendered dash and gap centers all the way around the circle,
  // including quadrant joins and the closing seam. Model properties alone
  // cannot catch malformed Canvas arcs or a dash pattern scaled by pen width.
  const auto image = capture_avatar(window, avatar);
  if (image.isNull()) return false;
  const auto scale = static_cast<qreal>(image.width()) / avatar->width();
  const auto radius = qMin(avatar->width(), avatar->height()) * 0.48;
  const QPointF center(avatar->width() / 2, avatar->height() / 2);
  bool starts_on_dash = false;
  for (int segment = 0; segment < 80; ++segment) {
    const auto angle = -std::numbers::pi / 2 + (segment + 0.5) * 2 * std::numbers::pi / 80;
    int opacity = 0;
    for (const auto offset : {-0.75, 0.0, 0.75}) {
      const auto local = center + QPointF(std::cos(angle), std::sin(angle)) * (radius + offset);
      const QPoint pixel(qFloor(local.x() * scale), qFloor(local.y() * scale));
      if (!expect(image.rect().contains(pixel), "The complete offline ring must remain visible")) return false;
      opacity = qMax(opacity, qAlpha(image.pixel(pixel)));
    }
    if (segment == 0) starts_on_dash = opacity > 90;
    if (!expect((opacity > 90) == ((segment % 2 == 0) == starts_on_dash),
                "Offline dashes and gaps must be evenly spaced")) {
      std::cerr << "Ring segment " << segment << " opacity " << opacity << '\n';
      return false;
    }
  }
  return true;
}

QString test_cover(const QString& assets, const QString& name, const QColor& color) {
  QByteArray png;
  if (!assets.isEmpty()) {
    QFile file(assets + '/' + name + ".png");
    if (file.open(QIODevice::ReadOnly)) png = file.readAll();
  }
  if (png.isEmpty()) {
    // Exercise realistic 600x800 artwork, not just one-pixel PNG signatures.
    QImage poster(600, 800, QImage::Format_RGB32);
    poster.fill(color);
    QPainter painter(&poster);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Segoe UI", 40, QFont::DemiBold));
    painter.drawText(poster.rect(), Qt::AlignCenter, name.toUpper());
    painter.end();
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    poster.save(&buffer, "PNG");
  }
  return "data:image/png;base64," + QString::fromLatin1(png.toBase64());
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  // Static Qt's offscreen plugin uses a generic font database. Point it at
  // Windows' own fonts so captures cannot silently pass with missing glyphs.
  if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) qputenv("QT_QPA_FONTDIR", qgetenv("SystemRoot") + "/Fonts");
#endif
  QGuiApplication app(argc, argv);
#ifdef Q_OS_WIN
  if (!expect(QFontDatabase::families().contains(QStringLiteral("Segoe UI")),
              "UI review must use real Windows fonts, not missing-glyph boxes"))
    return EXIT_FAILURE;
#endif
  QCoreApplication::setApplicationName(QStringLiteral("One UI test"));
  QCoreApplication::setOrganizationName(QStringLiteral("One tests"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QTemporaryDir settings;
  if (!settings.isValid()) return EXIT_FAILURE;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());

  QString screenshots;
  QString artwork_assets;
  for (const auto& argument : app.arguments()) {
    if (argument.startsWith(QStringLiteral("--screenshots="))) screenshots = argument.sliced(14);
    if (argument.startsWith(QStringLiteral("--artwork-dir="))) artwork_assets = argument.sliced(14);
  }
  if (!screenshots.isEmpty() && !QDir().mkpath(screenshots)) return EXIT_FAILURE;

  bool check_saved_appearance = false;
  for (const auto& mode : {QStringLiteral("desktop"), QStringLiteral("touch"), QStringLiteral("television")}) {
    QQmlApplicationEngine engine;
    bool warnings = false;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&warnings](const QList<QQmlError>& errors) {
      warnings = true;
      for (const auto& error : errors) std::cerr << error.toString().toStdString() << '\n';
    });
    QQmlComponent fixture_component(&engine, QUrl(QStringLiteral("qrc:/tests/ui_fixture.qml")));
    std::unique_ptr<QObject> fixture(fixture_component.create());
    if (!expect(fixture != nullptr, "UI fixture must load")) return EXIT_FAILURE;
    auto* environment = fixture->property("environment").value<QObject*>();
    auto* computers = fixture->property("computers").value<QObject*>();
    auto* applications = fixture->property("applications").value<QObject*>();
    auto* streaming = fixture->property("streaming").value<QObject*>();
    for (int row = 0; row < 2; ++row) {
      const auto cover =
          test_cover(artwork_assets, row == 0 ? "desktop" : "steam", QColor(row == 0 ? "#465260" : "#264D65"));
      QMetaObject::invokeMethod(applications, "setArtwork", Q_ARG(QVariant, row), Q_ARG(QVariant, cover),
                                Q_ARG(QVariant, ""));
    }
    environment->setProperty("mode", mode);
    engine.rootContext()->setContextProperty(QStringLiteral("UiEnvironment"), environment);
    engine.rootContext()->setContextProperty(QStringLiteral("ComputerModel"), computers);
    engine.rootContext()->setContextProperty(QStringLiteral("ApplicationModel"), applications);
    engine.rootContext()->setContextProperty(QStringLiteral("StreamingController"), streaming);
    engine.loadFromModule(QStringLiteral("One"), QStringLiteral("Main"));
    if (!expect(!engine.rootObjects().isEmpty(), "Main window must load")) return EXIT_FAILURE;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!expect(window != nullptr, "Root must be a window")) return EXIT_FAILURE;
    auto* theme = engine.singletonInstance<QObject*>("One", "Theme");
    if (check_saved_appearance &&
        !expect(theme->property("appearance").toInt() == 1, "Appearance must persist across UI restarts"))
      return EXIT_FAILURE;
    theme->setProperty("appearance", 0);
    QTest::qWait(250);
    if (!capture(window, screenshots, mode + "-computers")) return EXIT_FAILURE;
    const auto navigation = navigation_bounds(window);
    if (!expect_navigation(window, navigation) ||
        !expect(window->property("sidebarVisible").toBool() == (mode == "desktop"),
                "The desktop sidebar must also be present on the home page"))
      return EXIT_FAILURE;
    if (!expect(mode == "desktop" ? navigation[1].top() > navigation[0].bottom()
                                  : navigation[1].left() > navigation[0].right(),
                "Desktop navigation is vertical; TV and mobile navigation is horizontal"))
      return EXIT_FAILURE;
    if (!expect(mode == "desktop" || (mode == "television" ? navigation[0].bottom() <= 80
                                                           : navigation[0].top() >= window->height() - 80),
                "TV navigation belongs at the top and mobile navigation at the bottom"))
      return EXIT_FAILURE;

    auto* first_avatar = find_item(window->contentItem(), "hostAvatar0");
    if (!expect(first_avatar && first_avatar->property("fullMoon").toBool() &&
                    first_avatar->property("orbitColor").value<QColor>() == QColor("#F3F4F6"),
                "The selected online Sunshine instance must be a full moon with a white ring"))
      return EXIT_FAILURE;
    const auto full_moon = capture_avatar(window, first_avatar);
    QMetaObject::invokeMethod(computers, "setPaired", Q_ARG(QVariant, 0), Q_ARG(QVariant, false));
    QTest::qWait(50);
    const auto half_moon = capture_avatar(window, first_avatar);
    if (!expect(
            !first_avatar->property("fullMoon").toBool() && first_avatar->property("halfMoon").toBool() &&
                !full_moon.isNull() && !half_moon.isNull() && full_moon != half_moon &&
                find_item(window->contentItem(), "selectedHostAction")->property("text").toString() == "Pair computer",
            "An unpaired online host must repaint as a half moon and offer pairing"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-unpaired-selected")) return EXIT_FAILURE;
    QMetaObject::invokeMethod(computers, "setOnline", Q_ARG(QVariant, 0), Q_ARG(QVariant, false));
    QTest::qWait(50);
    if (!expect(!first_avatar->property("fullMoon").toBool() && !first_avatar->property("halfMoon").toBool() &&
                    first_avatar->property("orbitColor").value<QColor>() == QColor("#AAB0B8") &&
                    !find_item(window->contentItem(), "selectedHostAction")->isEnabled(),
                "A selected host going offline becomes a crescent with a gray ring and cannot connect"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-offline-selected") || !expect_even_offline_ring(window, first_avatar))
      return EXIT_FAILURE;
    QMetaObject::invokeMethod(computers, "setOnline", Q_ARG(QVariant, 0), Q_ARG(QVariant, true));
    QTest::qWait(50);
    if (!expect(first_avatar->property("halfMoon").toBool() &&
                    find_item(window->contentItem(), "selectedHostAction")->isEnabled(),
                "A returning unpaired host restores its half moon and pairing action"))
      return EXIT_FAILURE;
    QMetaObject::invokeMethod(computers, "setPaired", Q_ARG(QVariant, 0), Q_ARG(QVariant, true));
    QTest::qWait(50);
    if (!expect(first_avatar->property("fullMoon").toBool() && !first_avatar->property("halfMoon").toBool() &&
                    capture_avatar(window, first_avatar) == full_moon,
                "Pairing must restore the rendered full moon without recreating the page"))
      return EXIT_FAILURE;
    if (!click(window, "refreshComputers") ||
        !expect(computers->property("refreshCount").toInt() == 1, "Refresh must recheck known hosts"))
      return EXIT_FAILURE;

    auto* host_list = find_item(window->contentItem(), "hostGrid");
    if (!expect(host_list != nullptr, "Host carousel must be present")) return EXIT_FAILURE;
    if (mode == "touch" && !expect(qAbs(host_list->width() - host_list->property("tileWidth").toReal()) < 0.5,
                                   "Compact carousels show one complete host without cropped neighboring labels"))
      return EXIT_FAILURE;
    QMetaObject::invokeMethod(computers, "addExtraHosts");
    QTest::qWait(80);
    QMetaObject::invokeMethod(host_list, "focusHost", Q_ARG(QVariant, 0));
    QTest::qWait(80);
    if (mode == "touch") {
      const QPointF local_start(host_list->width() * 0.82, host_list->height() * 0.3);
      QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, host_list->mapToScene(local_start).toPoint());
      for (int step = 1; step <= 8; ++step) {
        const auto position = local_start - QPointF(host_list->width() * step * 0.075, 0);
        QTest::mouseMove(window, host_list->mapToScene(position).toPoint(), 20);
      }
      const auto finish = local_start - QPointF(host_list->width() * 0.6, 0);
      QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, host_list->mapToScene(finish).toPoint());
      QTest::qWait(1000);
      if (!expect(host_list->property("currentIndex").toInt() > 0 && computers->property("openedRow").toInt() == -1 &&
                      computers->property("pairedRow").toInt() == -1,
                  "Swiping the host carousel must update selection without opening or pairing"))
        return EXIT_FAILURE;
      QMetaObject::invokeMethod(host_list, "focusHost", Q_ARG(QVariant, 0));
      QTest::qWait(80);
    }
    QTest::keyClick(window, Qt::Key_End);
    QTest::qWait(100);
    auto* last_host = find_item(window->contentItem(), "hostAction8");
    if (!expect(last_host && last_host->hasActiveFocus() && host_list->property("currentIndex").toInt() == 8 &&
                    host_list->boundingRect().contains(last_host->mapRectToItem(host_list, last_host->boundingRect())),
                "End must scroll to and focus the last host, including on narrow screens"))
      return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(50);
    if (!expect(computers->property("openedRow").toInt() == -1 && computers->property("pairedRow").toInt() == -1 &&
                    !find_item(window->contentItem(), "selectedHostAction")->isEnabled(),
                "Offline profiles remain selectable but cannot connect or pair"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-many-hosts")) return EXIT_FAILURE;
    QMetaObject::invokeMethod(computers, "removeExtraHosts");
    QMetaObject::invokeMethod(host_list, "focusHost", Q_ARG(QVariant, 0));
    QTest::qWait(100);

    auto* offline = find_item(window->contentItem(), QStringLiteral("hostAction2"));
    if (mode != "touch") {
      auto* offline_avatar = find_item(window->contentItem(), "hostAvatar2");
      if (!expect(offline_avatar != nullptr, "Offline avatar must remain visible") ||
          !expect_even_offline_ring(window, offline_avatar))
        return EXIT_FAILURE;
      if (!expect(offline && offline->isEnabled(), "Offline hosts must remain selectable") ||
          !click(window, "hostAction2"))
        return EXIT_FAILURE;
      auto* selected_action = find_item(window->contentItem(), "selectedHostAction");
      if (!expect(selected_action && !selected_action->isEnabled() && computers->property("openedRow").toInt() == -1 &&
                      computers->property("pairedRow").toInt() == -1,
                  "Selecting an offline profile must never open or pair it"))
        return EXIT_FAILURE;
      offline->forceActiveFocus();
      QTest::keyClick(window, Qt::Key_Home);
      QTest::qWait(80);
      if (!expect(find_item(window->contentItem(), "hostAction0")->hasActiveFocus(), "Home must focus the first host"))
        return EXIT_FAILURE;
      QTest::keyClick(window, Qt::Key_Right);
      QTest::qWait(80);
      if (!expect(find_item(window->contentItem(), "hostAction1")->hasActiveFocus(),
                  "Right arrow must focus the next Sunshine instance"))
        return EXIT_FAILURE;
      if (!expect(find_item(window->contentItem(), "hostAvatar1")->property("halfMoon").toBool() &&
                      find_item(window->contentItem(), "hostAvatar0")->property("fullMoon").toBool() &&
                      !find_item(window->contentItem(), "hostAvatar0")->property("selected").toBool(),
                  "Pairing state, not selection, distinguishes the phases of online instances"))
        return EXIT_FAILURE;
    }
    if (!click(window, "addComputerButton")) return EXIT_FAILURE;
    auto* address = find_item(window->contentItem(), QStringLiteral("hostAddress"));
    if (!expect(address && address->hasActiveFocus(), "Add dialog must focus its address field")) return EXIT_FAILURE;
    for (const char character : QByteArrayLiteral("192.168.1.200")) QTest::keyClick(window, character);
    if (!capture(window, screenshots, mode + "-add")) return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(200);
    if (!expect(computers->property("addedAddress").toString() == QStringLiteral("192.168.1.200"),
                "Enter must add the host"))
      return EXIT_FAILURE;

    // Verify PIN display and result handling on a visible unpaired card.
    if (mode != "touch") {
      if (!click(window, "hostAction1")) return EXIT_FAILURE;
      if (!expect(computers->property("pairedRow").toInt() == 1, "Pair must target the selected host"))
        return EXIT_FAILURE;
      auto* dialog = window->findChild<QObject*>(QStringLiteral("pairingDialog"));
      if (!expect(dialog && dialog->property("pending").toBool() &&
                      dialog->property("pin").toString() == QStringLiteral("1234"),
                  "Pairing PIN must be visible"))
        return EXIT_FAILURE;
      if (!expect(dialog->property("instanceEndpoint").toString() == "192.168.1.105:48089",
                  "Pairing dialog must identify the selected instance's port"))
        return EXIT_FAILURE;
      if (!capture(window, screenshots, mode + "-pairing")) return EXIT_FAILURE;
      QMetaObject::invokeMethod(computers, "pairingCompleted", Q_ARG(QString, "studio"), Q_ARG(bool, true),
                                Q_ARG(QString, ""));
      if (!expect(!dialog->property("pending").toBool(), "Pair completion must replace the PIN")) return EXIT_FAILURE;
      QMetaObject::invokeMethod(dialog, "close");
      QTest::qWait(200);
    }

    if (!click(window, "hostAction0")) return EXIT_FAILURE;
    if (!expect(computers->property("openedRow").toInt() == 0, "View games must open the selected host"))
      return EXIT_FAILURE;
    auto* library_stack = window->findChild<QObject*>(QStringLiteral("computersStack"));
    auto* library_page = library_stack->property("currentItem").value<QObject*>();
    if (!expect(library_page->property("hostEndpoint").toString() == "192.168.1.105:47989",
                "Library must display its own instance endpoint"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-library")) return EXIT_FAILURE;
    if (!expect_navigation(window, navigation)) return EXIT_FAILURE;
    auto* grid = find_item(window->contentItem(), "gameGrid");
    if (!expect(grid && qAbs(grid->property("contentY").toReal()) < 0.1,
                "Opening the library must show the top of its first poster row"))
      return EXIT_FAILURE;
    for (int row = 0; row < 2; ++row) {
      auto* poster = find_item(window->contentItem(), "gamePoster" + QString::number(row));
      if (!expect(
              poster && qAbs(poster->width() / poster->height() - 0.75) < 0.001 && poster->property("ready").toBool(),
              "Desktop and Steam covers must decode into exact 3:4 posters"))
        return EXIT_FAILURE;
    }
    auto* second_card = find_item(window->contentItem(), "gameCard1");
    auto* first_card = find_item(window->contentItem(), "gameCard0");
    if (!expect(first_card && second_card && qAbs(first_card->y() - second_card->y()) < 0.1,
                "The first two posters must share a row at desktop, TV and phone widths"))
      return EXIT_FAILURE;
    second_card->forceActiveFocus();
    QTest::qWait(50);
    auto* selected_title = find_item(window->contentItem(), "selectedTitle");
    if (selected_title && !expect(selected_title->property("text").toString() == "Steam Big Picture",
                                  "Keyboard selection must update the title preview"))
      return EXIT_FAILURE;
    QMetaObject::invokeMethod(applications, "setArtwork", Q_ARG(QVariant, 1), Q_ARG(QVariant, ""),
                              Q_ARG(QVariant, "Sunshine returned an empty cover."));
    QTest::qWait(50);
    if (!click(window, "retryArtwork1") ||
        !expect(applications->property("retriedArtwork").toInt() == 2, "Cover retry must target its application"))
      return EXIT_FAILURE;
    QMetaObject::invokeMethod(applications, "setArtwork", Q_ARG(QVariant, 1),
                              Q_ARG(QVariant, test_cover(artwork_assets, "steam", QColor("#264D65"))),
                              Q_ARG(QVariant, ""));
    QTest::qWait(100);
    if (!click(window, "refreshApplications")) return EXIT_FAILURE;
    if (!expect(applications->property("refreshCount").toInt() == 1, "Refresh must call the application model"))
      return EXIT_FAILURE;
    applications->setProperty("loading", true);
    QTest::qWait(50);
    if (!expect(!find_item(window->contentItem(), "refreshApplications")->isEnabled(), "Loading must disable refresh"))
      return EXIT_FAILURE;
    applications->setProperty("loading", false);
    applications->setProperty("error", "This computer is no longer reachable. Try again when it is online.");
    QTest::qWait(50);
    if (!capture(window, screenshots, mode + "-library-error")) return EXIT_FAILURE;
    applications->setProperty("pairingRequired", true);
    QTest::qWait(50);
    if (!expect(!find_item(window->contentItem(), "refreshApplications")->isEnabled(),
                "Unpaired library must disable refresh"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-library-unpaired")) return EXIT_FAILURE;
    if (!click(window, "libraryRecovery") || !click(window, "hostAction0")) return EXIT_FAILURE;
    applications->setProperty("pairingRequired", false);
    applications->setProperty("error", "");
    QTest::qWait(50);
    if (!click(window, "gameCard0")) return EXIT_FAILURE;
    if (!expect(streaming->property("startedId").toInt() == 1, "Game card must launch its own application ID"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-connecting")) return EXIT_FAILURE;
    // Back through the real page signal also exercises streaming teardown.
    auto* stack = window->findChild<QObject*>(QStringLiteral("computersStack"));
    if (!expect(stack != nullptr, "Computers stack must exist")) return EXIT_FAILURE;
    auto* stream_page = stack->property("currentItem").value<QObject*>();
    QMetaObject::invokeMethod(stream_page, "leaveStream");
    QTest::qWait(300);
    if (!expect(streaming->property("stopCount").toInt() > 0, "Leaving the stream must stop it")) return EXIT_FAILURE;

    if (!click(window, "navigation1")) return EXIT_FAILURE;
    if (!expect_navigation(window, navigation)) return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-settings")) return EXIT_FAILURE;
    QTest::keyClick(window, mode == "desktop" ? Qt::Key_Right : Qt::Key_Down);
    QTest::qWait(50);
    if (!expect(find_item(window->contentItem(), "appearance0")->hasActiveFocus(),
                "Navigation must transfer keyboard focus to page controls"))
      return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Right);
    if (!expect(find_item(window->contentItem(), "appearance1")->hasActiveFocus(),
                "Arrow keys must move between appearance options"))
      return EXIT_FAILURE;
    QTest::keyClick(window, Qt::Key_Space);
    QTest::qWait(100);
    if (!expect(window->color() == QColor("#F3F6F5"), "Light appearance must change the window palette"))
      return EXIT_FAILURE;
    if (!expect(theme->property("accentText").value<QColor>() == QColor("#FFFFFF"),
                "Light primary buttons need white text"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-light-settings")) return EXIT_FAILURE;
    if (!click(window, "navigation0")) return EXIT_FAILURE;
    if (mode != "television") {
      const auto size = window->size();
      window->resize(mode == "touch" ? QSize(360, 600) : QSize(800, 600));
      QTest::qWait(150);
      auto* small_poster = find_item(window->contentItem(), "gamePoster0");
      if (!expect(small_poster && qAbs(small_poster->width() / small_poster->height() - 0.75) < 0.001,
                  "Small layouts must retain 3:4 poster proportions"))
        return EXIT_FAILURE;
      if (!capture(window, screenshots, mode + "-small-library")) return EXIT_FAILURE;
      window->resize(size);
      QTest::qWait(150);
    }
    if (!capture(window, screenshots, mode + "-light-library")) return EXIT_FAILURE;
    if (!click(window, "backToComputers")) return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-light-computers")) return EXIT_FAILURE;
    if (!expect(find_item(window->contentItem(), "hostAvatar0")->property("orbitColor").value<QColor>() ==
                    QColor("#626B78"),
                "Light appearance needs a darker neutral ring for contrast"))
      return EXIT_FAILURE;
    if (!expect_navigation(window, navigation)) return EXIT_FAILURE;
    if (!click(window, "navigation1") || !click(window, "appearance0") || !click(window, "navigation2"))
      return EXIT_FAILURE;
    if (!capture(window, screenshots, mode + "-about")) return EXIT_FAILURE;
    if (!expect_navigation(window, navigation)) return EXIT_FAILURE;

    if (!click(window, "navigation0")) return EXIT_FAILURE;
    if (mode != "television") {
      const auto size = window->size();
      window->resize(mode == "touch" ? QSize(360, 600) : QSize(800, 600));
      QTest::qWait(150);
      if (!capture(window, screenshots, mode + "-small-computers")) return EXIT_FAILURE;
      auto* action = find_item(window->contentItem(), QStringLiteral("hostAction0"));
      if (!expect(action != nullptr, "Small windows must retain their host action")) return EXIT_FAILURE;
      for (auto* ancestor = action->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
        if (ancestor->clip() &&
            !expect(ancestor->boundingRect().contains(action->mapRectToItem(ancestor, action->boundingRect())),
                    "Small window must not clip the host action"))
          return EXIT_FAILURE;
      }
      const auto compact_navigation = navigation_bounds(window);
      for (const auto* destination : {"navigation1", "navigation2", "navigation0"}) {
        if (!click(window, destination) || !expect_navigation(window, compact_navigation)) return EXIT_FAILURE;
      }
      QMetaObject::invokeMethod(computers, "setOnline", Q_ARG(QVariant, 0), Q_ARG(QVariant, false));
      QTest::qWait(100);
      if (!capture(window, screenshots, mode + "-small-offline") ||
          !expect_even_offline_ring(window, find_item(window->contentItem(), "hostAvatar0")))
        return EXIT_FAILURE;
      QMetaObject::invokeMethod(computers, "setOnline", Q_ARG(QVariant, 0), Q_ARG(QVariant, true));
      window->resize(size);
      QTest::qWait(150);
    }
    QMetaObject::invokeMethod(computers, "clear");
    QTest::qWait(150);
    if (!capture(window, screenshots, mode + "-empty")) return EXIT_FAILURE;
    if (mode == "touch") {
      window->resize(360, 600);
      QTest::qWait(150);
      if (!capture(window, screenshots, mode + "-small-empty")) return EXIT_FAILURE;
    }
    if (!expect(!warnings, "UI must not emit QML binding, layout, or runtime warnings")) return EXIT_FAILURE;
    // Destroy the window before its injected model objects.
    theme->setProperty("appearance", 1);
    check_saved_appearance = true;
    delete window;
    std::cout << mode.toStdString() << " UI interactions passed\n";
  }
  return EXIT_SUCCESS;
}
