#include "ui/panetitlebar.h"

namespace ui {

PaneTitleBar::PaneTitleBar(const QString &title, QWidget *parent)
    : QLabel(parent)
{
    setObjectName(QStringLiteral("paneTitle"));
    setTitle(title);
    setFixedHeight(22);
}

void PaneTitleBar::setTitle(const QString &title)
{
    setText(title.toUpper());
}

} // namespace ui
