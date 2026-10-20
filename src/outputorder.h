#pragma once

#include <QObject>
#include <QStringList>

#include <memory>

class QScreen;
class OutputOrderProtocol;

/**
 * KWin's output order (kde_output_order_v1), the same source plasmashell uses.
 * QGuiApplication::primaryScreen() does not follow Plasma's primary setting on Wayland.
 */
class OutputOrder : public QObject
{
    Q_OBJECT

public:
    explicit OutputOrder(QObject *parent = nullptr);
    ~OutputOrder() override;

    bool isSupported() const;
    /// Output names, primary first. Empty until the compositor has sent the order.
    QStringList order() const { return m_order; }
    /// Plasma's primary screen, falling back to Qt's idea of it.
    QScreen *primaryScreen() const;

Q_SIGNALS:
    void orderChanged();

private:
    friend class OutputOrderProtocol;
    void setOrder(const QStringList &order);

    std::unique_ptr<OutputOrderProtocol> m_protocol;
    QStringList m_order;
};
