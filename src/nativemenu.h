#pragma once

#include <QJSValue>
#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class QMenu;
class QQuickItem;
class QQuickWindow;

/**
 * A real KDE context menu (QMenu), the same component Plasma uses for its desktop and
 * panel menus, so the dock's menus look and behave exactly like the rest of the
 * desktop and follow the user's style and theme.
 *
 * Built from QML: addAction() takes a JavaScript callback, addMenu() returns a
 * submenu (fill it from its aboutToShow for lazy content), popupAbove() opens it
 * centred above a dock item.
 */
class NativeMenu : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit NativeMenu(QObject *parent = nullptr);
    ~NativeMenu() override;

    /// options: enabled (bool), bold (bool), checkable (bool), checked (bool)
    Q_INVOKABLE void addAction(const QString &text, const QString &icon, const QJSValue &callback, const QVariantMap &options = {});
    /// Skipped at the top and when the last entry is already a separator.
    Q_INVOKABLE void addSeparator();
    Q_INVOKABLE NativeMenu *addMenu(const QString &title, const QString &icon);
    Q_INVOKABLE bool isEmpty() const;

    /// Opens the menu centred above `item`, `gap` pixels clear of it.
    Q_INVOKABLE void popupAbove(QQuickItem *item, int gap = 8);
    Q_INVOKABLE void close();

Q_SIGNALS:
    void aboutToShow();
    void aboutToHide();

private:
    // Wraps an existing submenu of a parent NativeMenu.
    NativeMenu(QMenu *menu, QObject *parent);
    void connectMenu();

    /// Lets go of the press that opened the menu (see popupAbove()).
    void releaseDockPointer();

    QPointer<QMenu> m_menu;
    bool m_ownsMenu = true;
    QPointer<QQuickWindow> m_dockWindow;
};
