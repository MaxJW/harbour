#pragma once

#include <QDBusVariant>
#include <QObject>

class DockManager;
class HarbourSettings;

/**
 * The dock's D-Bus interface (service dev.harbour.dock, object /Dock).
 *
 * The System Settings page uses preview()/endPreview() so changes such as the icon
 * size show on the dock while a slider is being dragged; reveal() brings a dock up
 * for a moment (also handy for scripted tests).
 */
class DockService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "dev.harbour.Dock")

public:
    DockService(DockManager *docks, HarbourSettings *settings, QObject *parent = nullptr);

    /// Claims the service name. False if another Harbour already runs.
    bool registerOnBus();

public Q_SLOTS:
    /// Applies a setting (by its kcfg name, e.g. "iconSize") without saving it, and
    /// shows the dock for a moment so the change can be seen.
    void preview(const QString &name, const QDBusVariant &value);
    /// Drops unsaved previews by re-reading the settings file.
    void endPreview();
    /// Shows the dock on the given screen (or the active one if empty) for a moment.
    void reveal(const QString &screen);

private:
    DockManager *m_docks;
    HarbourSettings *m_settings;
};
