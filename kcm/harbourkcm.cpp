#include "harbourkcm.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusVariant>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(HarbourKcm, "kcm_harbour.json")

namespace
{
const auto GenieId = u"harbour_genie"_s;
const auto MagicLampId = u"magiclamp"_s;

void callEffects(const QString &method, const QString &effect)
{
    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/Effects"_s, u"org.kde.kwin.Effects"_s, method);
    message << effect;
    QDBusConnection::sessionBus().asyncCall(message);
}

// The running dock's interface (see DockService); fire and forget.
void callDock(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(u"dev.harbour.dock"_s, u"/Dock"_s, u"dev.harbour.Dock"_s, method);
    message.setArguments(arguments);
    QDBusConnection::sessionBus().asyncCall(message);
}
}

HarbourKcm::HarbourKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_settings(new HarbourSettings(this))
    , m_genieInstalled(KPluginMetaData::findPluginById(u"kwin/effects/plugins"_s, GenieId).isValid())
{
    setButtons(Apply | Default);
}

HarbourKcm::~HarbourKcm()
{
    // Closing without applying: the dock goes back to the saved values.
    callDock(u"endPreview"_s, {});
}

void HarbourKcm::preview(const QString &name, const QVariant &value)
{
    callDock(u"preview"_s, {name, QVariant::fromValue(QDBusVariant(value))});
}

QString HarbourKcm::readMinimizeEffect() const
{
    const KSharedConfig::Ptr kwinrc = KSharedConfig::openConfig(u"kwinrc"_s);
    if (!m_genieInstalled || !kwinrc->group(u"Plugins"_s).readEntry(GenieId + u"Enabled"_s, false)) {
        return u"Default"_s;
    }
    return kwinrc->group(u"Effect-harbour_genie"_s).readEntry("Style", u"Genie"_s);
}

void HarbourKcm::load()
{
    KQuickManagedConfigModule::load();
    callDock(u"endPreview"_s, {});
    m_savedMinimizeEffect = readMinimizeEffect();
    setMinimizeEffect(m_savedMinimizeEffect);
}

void HarbourKcm::save()
{
    KQuickManagedConfigModule::save();

    if (m_minimizeEffect != m_savedMinimizeEffect) {
        const KSharedConfig::Ptr kwinrc = KSharedConfig::openConfig(u"kwinrc"_s);
        KConfigGroup plugins = kwinrc->group(u"Plugins"_s);
        const bool useGenie = m_minimizeEffect != u"Default";
        if (useGenie) {
            kwinrc->group(u"Effect-harbour_genie"_s).writeEntry("Style", m_minimizeEffect);
            plugins.writeEntry(GenieId + u"Enabled"_s, true);
            // Only one minimise animation at a time (they share KWin's "minimize" group).
            plugins.writeEntry(MagicLampId + u"Enabled"_s, false);
        } else {
            plugins.writeEntry(GenieId + u"Enabled"_s, false);
        }
        kwinrc->sync();

        if (useGenie) {
            callEffects(u"unloadEffect"_s, MagicLampId);
            callEffects(u"loadEffect"_s, GenieId);
            callEffects(u"reconfigureEffect"_s, GenieId);
        } else {
            callEffects(u"unloadEffect"_s, GenieId);
        }
        m_savedMinimizeEffect = m_minimizeEffect;
    }
    // The file now holds the previewed values; have the dock read it back.
    callDock(u"endPreview"_s, {});
    settingsChanged();
}

void HarbourKcm::defaults()
{
    KQuickManagedConfigModule::defaults();
    setMinimizeEffect(m_genieInstalled ? u"Genie"_s : u"Default"_s);
}

void HarbourKcm::setMinimizeEffect(const QString &effect)
{
    if (m_minimizeEffect == effect) {
        return;
    }
    m_minimizeEffect = effect;
    Q_EMIT minimizeEffectChanged();
    settingsChanged();
}

bool HarbourKcm::isSaveNeeded() const
{
    return m_minimizeEffect != m_savedMinimizeEffect;
}

bool HarbourKcm::isDefaults() const
{
    return m_minimizeEffect == (m_genieInstalled ? u"Genie"_s : u"Default"_s);
}

#include "harbourkcm.moc"
