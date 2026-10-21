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
#include <QQuickWindow>
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

    // Drawn with Qt Quick's software renderer into shared memory, which KWin composites
    // like any other window. A GPU context per window (both docks, each stack popup) cost
    // ~25 MB apiece on NVIDIA, never given back after a popup closed: 70 MB at start
    // and 200 MB after some use, against ~30 and ~40 now. The dock is small 2D content
    // repainted only where it changes, so frames take well under a millisecond anyway.
    // QT_QUICK_BACKEND or QSG_RHI_BACKEND still pick a GPU backend.
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND") && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    // KIO would otherwise run its workers as threads in this process: listing a stack
    // or making its previews then loads every thumbnailer and image codec into the dock.
    if (!qEnvironmentVariableIsSet("KIO_ENABLE_WORKER_THREADS")) {
        qputenv("KIO_ENABLE_WORKER_THREADS", "0");
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
    engine.addImageProvider(u"harbourpreview"_s, new PreviewProvider);

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
