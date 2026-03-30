#include "MessageModel.h"

#include <QDateTime>

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const MessageItem &item = m_items.at(index.row());

    switch (role) {
    case SourceRole:
        return item.source;
    case TextRole:
        return item.text;
    case KindRole:
        return item.kind;
    case TimeRole:
        return item.time;
    default:
        return {};
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    return {
        {SourceRole, "source"},
        {TextRole, "text"},
        {KindRole, "kind"},
        {TimeRole, "time"},
    };
}

void MessageModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, m_items.size() - 1);
    m_items.clear();
    endRemoveRows();
}

void MessageModel::addMessage(const QString &source, const QString &text, const QString &kind)
{
    addMessageWithRow(source, text, kind);
}

int MessageModel::addMessageWithRow(const QString &source, const QString &text, const QString &kind)
{
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.push_back(
        MessageItem{
            source,
            text,
            kind,
            QDateTime::currentDateTime().toString("hh:mm:ss"),
        });
    endInsertRows();
    return row;
}

bool MessageModel::appendTextAt(int row, const QString &suffix)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }

    if (suffix.isEmpty()) {
        return true;
    }

    m_items[row].text += suffix;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {TextRole});
    return true;
}

bool MessageModel::setTextAt(int row, const QString &text)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }

    m_items[row].text = text;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {TextRole});
    return true;
}
