#include "nativemenu.h"

#include <QIcon>
#include <QMenu>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <qpa/qplatformwindow_p.h>

NativeMenu::NativeMenu(QObject *parent)
    : QObject(parent)
    , m_menu(new QMenu)
{
    connectMenu();
}

NativeMenu::NativeMenu(QMenu *menu, QObject *parent)
    : QObject(parent)
    , m_menu(menu)
    , m_ownsMenu(false)
{
    connectMenu();
}

NativeMenu::~NativeMenu()
{
    if (m_ownsMenu && m_menu) {
        // Deleted later: this can run from inside the menu's own signal handling.
        m_menu->deleteLater();
    }
}

void NativeMenu::connectMenu()
{
    connect(m_menu, &QMenu::aboutToShow, this, &NativeMenu::aboutToShow);
    connect(m_menu, &QMenu::aboutToHide, this, &NativeMenu::aboutToHide);
}

void NativeMenu::addAction(const QString &text, const QString &icon, const QJSValue &callback, const QVariantMap &options)
{
    if (!m_menu) {
        return;
    }
    QAction *action = m_menu->addAction(QIcon::fromTheme(icon), text);
    action->setEnabled(options.value(QStringLiteral("enabled"), true).toBool());
    if (options.value(QStringLiteral("checkable")).toBool()) {
        action->setCheckable(true);
        action->setChecked(options.value(QStringLiteral("checked")).toBool());
    }
    if (options.value(QStringLiteral("bold")).toBool()) {
        QFont font = action->font();
        font.setBold(true);
        action->setFont(font);
    }
    if (callback.isCallable()) {
        connect(action, &QAction::triggered, this, [callback]() mutable {
            callback.call();
        });
    }
}

void NativeMenu::addSeparator()
{
    if (!m_menu || m_menu->isEmpty() || m_menu->actions().constLast()->isSeparator()) {
        return;
    }
    m_menu->addSeparator();
}

NativeMenu *NativeMenu::addMenu(const QString &title, const QString &icon)
{
    if (!m_menu) {
        return nullptr;
    }
    auto *submenu = new NativeMenu(m_menu->addMenu(QIcon::fromTheme(icon), title), this);
    QQmlEngine::setObjectOwnership(submenu, QQmlEngine::CppOwnership);
    return submenu;
}

bool NativeMenu::isEmpty() const
{
    return !m_menu || m_menu->isEmpty();
}

void NativeMenu::popupAbove(QQuickItem *item, int gap)
{
    if (!m_menu || !item || !item->window()) {
        return;
    }
    // No trailing separator.
    if (!m_menu->isEmpty() && m_menu->actions().constLast()->isSeparator()) {
        m_menu->removeAction(m_menu->actions().constLast());
    }

    // A popup on Wayland needs to know the window it belongs to.
    m_menu->createWinId();
    m_menu->windowHandle()->setTransientParent(item->window());

    // Styled first: the style's frame and margins only count once the menu is polished.
    m_menu->ensurePolished();
    m_menu->adjustSize();
    const QSize size = m_menu->size();

    // Wayland never tells a client where its windows are, so QMenu's own placement
    // (which keeps menus on screen using the parent's believed position) is wrong for
    // a dock at the bottom of the screen. Instead attach the menu to a rectangle in the
    // dock window: as wide as the menu and centred over the item, and ask for the menu
    // to grow upwards from its top. Left to the defaults it would open downwards
    // whenever it fits below the anchor (small menus do), covering the icon.
    const QPointF itemTop = item->mapToScene(QPointF(item->width() / 2, 0));
    const QRect anchor(qRound(itemTop.x() - size.width() / 2.0), qRound(itemTop.y()) - gap, size.width(), 1);
    QWindow *menuWindow = m_menu->windowHandle();
    menuWindow->setProperty("_q_waylandPopupAnchorRect", anchor);
    menuWindow->setProperty("_q_waylandPopupAnchor", QVariant::fromValue(Qt::Edges(Qt::TopEdge)));
    menuWindow->setProperty("_q_waylandPopupGravity", QVariant::fromValue(Qt::Edges(Qt::TopEdge)));
    // Slide along the screen edges rather than flipping down over the dock.
    menuWindow->setProperty("_q_waylandPopupConstraintAdjustment", 1u | 2u); // slide_x | slide_y
    if (auto waylandWindow = menuWindow->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        waylandWindow->setExtendedWindowType(QNativeInterface::Private::QWaylandWindow::Menu);
        waylandWindow->setParentControlGeometry(anchor);
    }
    m_menu->popup(item->window()->mapToGlobal(anchor.topLeft()));
}

void NativeMenu::close()
{
    if (m_menu) {
        m_menu->close();
    }
}
