#pragma once

#include "harboursettings.h"

#include <KQuickManagedConfigModule>

/**
 * The Dock page in System Settings, laid out like macOS's Desktop & Dock.
 *
 * Everything except the minimise animation is a HarbourSettings entry: the dock
 * re-reads harbourrc whenever it changes, so edits apply live. The minimise
 * animation belongs to KWin (kwinrc), so it is handled here by hand.
 */
class HarbourKcm : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(HarbourSettings *settings READ settings CONSTANT)
    /// Whether the Harbour Genie KWin effect is installed.
    Q_PROPERTY(bool genieInstalled READ genieInstalled CONSTANT)
    /// "Genie", "Scale", "Suck" (Harbour's effect), or "Default" (KWin's own, e.g. Magic Lamp).
    Q_PROPERTY(QString minimizeEffect READ minimizeEffect WRITE setMinimizeEffect NOTIFY minimizeEffectChanged)

public:
    HarbourKcm(QObject *parent, const KPluginMetaData &metaData);
    ~HarbourKcm() override;

    /// Shows a not-yet-applied value on the running dock (which comes up to show it).
    Q_INVOKABLE void preview(const QString &name, const QVariant &value);

    /// A stack's path ("~/…" inside the home directory) for a folder picked in a dialog.
    Q_INVOKABLE QString stackPath(const QUrl &folder) const;
    /// The theme icon for a stack's folder (Downloads, Documents, … get their own).
    Q_INVOKABLE QString stackIcon(const QString &path) const;

    HarbourSettings *settings() const { return m_settings; }
    bool genieInstalled() const { return m_genieInstalled; }
    QString minimizeEffect() const { return m_minimizeEffect; }
    void setMinimizeEffect(const QString &effect);

    void load() override;
    void save() override;
    void defaults() override;

Q_SIGNALS:
    void minimizeEffectChanged();

protected:
    bool isSaveNeeded() const override;
    bool isDefaults() const override;

private:
    QString readMinimizeEffect() const;

    HarbourSettings *m_settings;
    bool m_genieInstalled = false;
    QString m_minimizeEffect;
    QString m_savedMinimizeEffect;
};
