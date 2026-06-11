#pragma once

#include "session/modulenode.h"

#include <QListWidget>

namespace ui {

class LogWidget : public QListWidget {
    Q_OBJECT
public:
    explicit LogWidget(QWidget *parent = nullptr);

    void setEntries(const QList<session::LogEntry> &entries);
    void appendEntry(int level, const QString &text);
    QString plainText() const;

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};

} // namespace ui
