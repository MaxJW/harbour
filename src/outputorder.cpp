#include "outputorder.h"

#include "qwayland-kde-output-order-v1.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

class OutputOrderProtocol : public QWaylandClientExtensionTemplate<OutputOrderProtocol>, public QtWayland::kde_output_order_v1
{
public:
    explicit OutputOrderProtocol(OutputOrder *owner)
        : QWaylandClientExtensionTemplate<OutputOrderProtocol>(1)
        , m_owner(owner)
    {
        initialize();
    }

    ~OutputOrderProtocol() override
    {
        if (isActive()) {
            destroy();
        }
    }

protected:
    void kde_output_order_v1_output(const QString &outputName) override
    {
        m_pending.append(outputName);
    }

    void kde_output_order_v1_done() override
    {
        m_owner->setOrder(std::exchange(m_pending, {}));
    }

private:
    OutputOrder *m_owner;
    QStringList m_pending;
};

OutputOrder::OutputOrder(QObject *parent)
    : QObject(parent)
    , m_protocol(std::make_unique<OutputOrderProtocol>(this))
{
}

OutputOrder::~OutputOrder() = default;

bool OutputOrder::isSupported() const
{
    return m_protocol->isActive();
}

QScreen *OutputOrder::primaryScreen() const
{
    const auto screens = QGuiApplication::screens();
    // Development override, e.g. HARBOUR_SCREEN=HDMI-A-1.
    if (const QString forced = qEnvironmentVariable("HARBOUR_SCREEN"); !forced.isEmpty()) {
        for (QScreen *screen : screens) {
            if (screen->name() == forced) {
                return screen;
            }
        }
    }
    for (const QString &name : m_order) {
        for (QScreen *screen : screens) {
            if (screen->name() == name) {
                return screen;
            }
        }
    }
    return QGuiApplication::primaryScreen();
}

void OutputOrder::setOrder(const QStringList &order)
{
    if (m_order == order) {
        return;
    }
    m_order = order;
    Q_EMIT orderChanged();
}
