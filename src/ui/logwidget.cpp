#include "ui/logwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>

namespace ui {

LogWidget::LogWidget(QWidget *parent)
    : QListWidget(parent)
{
    setObjectName(QStringLiteral("logList"));
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setUniformItemSizes(true);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    QFont mono(QStringLiteral("Consolas"));
    mono.setPointSizeF(9.0);
    setFont(mono);
}

void LogWidget::setEntries(const QList<session::LogEntry> &entries)
{
    clear();
    for (const auto &e : entries)
        appendEntry(e.level, e.text);
}

void LogWidget::appendEntry(int level, const QString &text)
{
    auto *item = new QListWidgetItem(text, this);
    switch (level) {
    case session::LogEntry::Error:
        item->setForeground(QColor(0xf4, 0x87, 0x71));
        break;
    case session::LogEntry::Warning:
        item->setForeground(QColor(0xcc, 0xa7, 0x00));
        break;
    default:
        item->setForeground(QColor(0x85, 0x85, 0x85));
        break;
    }
}

QString LogWidget::plainText() const
{
    QStringList lines;
    lines.reserve(count());
    for (int i = 0; i < count(); ++i)
        lines.append(item(i)->text());
    return lines.join(QLatin1Char('\n'));
}

void LogWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("Copy"));
    QAction *copyAllAction = menu.addAction(tr("Copy All"));
    menu.addSeparator();
    QAction *clearAction = menu.addAction(tr("Clear Log"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == copyAction) {
        QStringList lines;
        const auto items = selectedItems();
        for (const auto *it : items)
            lines.append(it->text());
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    } else if (chosen == copyAllAction) {
        QApplication::clipboard()->setText(plainText());
    } else if (chosen == clearAction) {
        clear();
    }
}

} // namespace ui
