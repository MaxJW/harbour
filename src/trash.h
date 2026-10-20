#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

/// The Trash at the end of the dock: whether it's empty, opening, emptying, and dropping onto it.
class Trash : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool empty READ isEmpty NOTIFY emptyChanged)

public:
    explicit Trash(QObject *parent = nullptr);

    bool isEmpty() const { return m_empty; }

    Q_INVOKABLE void open() const;
    /// Asks for confirmation the way Dolphin does (per the user's KIO settings).
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE void moveToTrash(const QList<QUrl> &urls);

Q_SIGNALS:
    void emptyChanged();

private:
    void refresh();

    // ⚠ Only the home trash is watched; trash folders on other drives don't update the icon.
    QString m_filesPath;
    QFileSystemWatcher m_watcher;
    bool m_empty = true;
};
