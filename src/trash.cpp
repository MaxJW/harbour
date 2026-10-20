#include "trash.h"

#include <KIO/DeleteOrTrashJob>
#include <KIO/OpenUrlJob>

#include <QDir>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

Trash::Trash(QObject *parent)
    : QObject(parent)
    , m_filesPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/Trash/files"_s)
{
    QDir().mkpath(m_filesPath);
    m_watcher.addPath(m_filesPath);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Trash::refresh);
    refresh();
}

void Trash::refresh()
{
    const bool empty = QDir(m_filesPath).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    if (empty != m_empty) {
        m_empty = empty;
        Q_EMIT emptyChanged();
    }
}

void Trash::open() const
{
    (new KIO::OpenUrlJob(QUrl(u"trash:/"_s)))->start();
}

void Trash::emptyTrash()
{
    auto *job = new KIO::DeleteOrTrashJob({}, KIO::AskUserActionInterface::EmptyTrash, KIO::AskUserActionInterface::DefaultConfirmation, this);
    job->start();
}

void Trash::moveToTrash(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    auto *job = new KIO::DeleteOrTrashJob(urls, KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation, this);
    job->start();
}
