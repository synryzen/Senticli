#pragma once

#include <QAbstractListModel>
#include <QVector>

struct MessageItem {
    QString source;
    QString text;
    QString kind;
    QString time;
};

class MessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        SourceRole = Qt::UserRole + 1,
        TextRole,
        KindRole,
        TimeRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();
    int addMessageWithRow(const QString &source, const QString &text, const QString &kind);
    void addMessage(const QString &source, const QString &text, const QString &kind);
    bool appendTextAt(int row, const QString &suffix);
    bool setTextAt(int row, const QString &text);

private:
    QVector<MessageItem> m_items;
};
