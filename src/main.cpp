#include "dockmanager.h"
#include "dockservice.h"
#include "docktools.h"
#include "harboursettings.h"
#include "outputorder.h"
#include "settingswatcher.h"

#include <KGlobalAccel>

#include <QAction>
#include <QApplication>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QScreen>
#include <QTimer>

#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    // Qt Quick's default animation driver advances by one vsync of Qt's primary screen
    // per frame. That screen isn't necessarily the dock's (here 60 Hz vs 165 Hz), which
    // made every animation run 2.75x too fast. Advance by wall-clock time instead.
    if (!qEnvironmentVariableIsSet("QSG_USE_SIMPLE_ANIMATION_DRIVER")) {
        qputenv("QSG_USE_SIMPLE_ANIMATION_DRIVER", "1");
    }

    // QApplication rather than QGuiApplication: the KDE desktop style for menus needs QStyle.
    QApplication app(argc, argv);
    app.setApplicationName(u"harbour"_s);
    app.setApplicationDisplayName(u"Harbour"_s);
    app.setDesktopFileName(u"dev.harbour.dock"_s);
    app.setApplicationVersion(QStringLiteral(HARBOUR_VERSION));
    app.setQuitOnLastWindowClosed(false);

    if (!QGuiApplication::platformName().startsWith(u"wayland"_s)) {
        qCritical("Harbour needs a Wayland session (platform is %s)", qPrintable(QGuiApplication::platformName()));
        return 1;
    }

    if (QQuickStyle::name().isEmpty()) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    HarbourSettings *settings = HarbourSettings::self();
    SettingsWatcher settingsWatcher(settings);

    // Meta+Alt+D, like ⌥⌘D on macOS, turns auto-hide on and off.
    QAction toggleAutoHide;
    toggleAutoHide.setObjectName(u"toggle-autohide"_s);
    toggleAutoHide.setText(u"Turn dock hiding on or off"_s);
    KGlobalAccel::self()->setDefaultShortcut(&toggleAutoHide, {QKeySequence(Qt::META | Qt::ALT | Qt::Key_D)});
    KGlobalAccel::self()->setShortcut(&toggleAutoHide, {QKeySequence(Qt::META | Qt::ALT | Qt::Key_D)});
    QObject::connect(&toggleAutoHide, &QAction::triggered, settings, [settings] {
        settings->setAutoHide(!settings->autoHide());
        settings->save();
    });

    QQmlEngine engine;
    engine.addImageProvider(u"harbour"_s, new IconProvider);

    OutputOrder outputOrder;
    DockManager docks(&engine, settings, &outputOrder);

    // Owning the D-Bus name also keeps Harbour to a single instance.
    DockService service(&docks, settings);
    if (!service.registerOnBus()) {
        qInfo("Harbour is already running");
        return 0;
    }

    if (outputOrder.isSupported()) {
        // Wait for KWin's output order so the right dock starts active; it arrives right
        // after binding, but don't wait forever if it doesn't.
        bool started = false;
        auto start = [&] {
            if (!std::exchange(started, true)) {
                docks.start();
            }
        };
        QObject::connect(&outputOrder, &OutputOrder::orderChanged, &app, start, Qt::SingleShotConnection);
        QTimer::singleShot(500ms, &app, start);
        return app.exec();
    }

    docks.start();

    return app.exec();
}
