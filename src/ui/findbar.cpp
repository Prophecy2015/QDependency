#include "ui/findbar.h"
#include "ui/iconfactory.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace ui {

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("findBar"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto *icon = new QLabel(this);
    icon->setPixmap(IconFactory::glyphIcon(QChar(0xE721)).pixmap(14, 14));
    layout->addWidget(icon);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Find (functions and modules)"));
    m_edit->setClearButtonEnabled(true);
    m_edit->setFixedWidth(280);
    layout->addWidget(m_edit);

    m_matchLabel = new QLabel(this);
    m_matchLabel->setObjectName(QStringLiteral("findMatchLabel"));
    layout->addWidget(m_matchLabel);

    auto *prevBtn = new QToolButton(this);
    prevBtn->setIcon(IconFactory::glyphIcon(QChar(0xE70E)));
    prevBtn->setToolTip(tr("Previous match (Shift+F3)"));
    layout->addWidget(prevBtn);

    auto *nextBtn = new QToolButton(this);
    nextBtn->setIcon(IconFactory::glyphIcon(QChar(0xE70D)));
    nextBtn->setToolTip(tr("Next match (F3)"));
    layout->addWidget(nextBtn);

    layout->addStretch();

    auto *closeBtn = new QToolButton(this);
    closeBtn->setIcon(IconFactory::glyphIcon(QChar(0xE711)));
    closeBtn->setToolTip(tr("Close (Esc)"));
    layout->addWidget(closeBtn);

    connect(m_edit, &QLineEdit::textChanged, this, &FindBar::searchChanged);
    connect(m_edit, &QLineEdit::returnPressed, this, &FindBar::findNext);
    connect(nextBtn, &QToolButton::clicked, this, &FindBar::findNext);
    connect(prevBtn, &QToolButton::clicked, this, &FindBar::findPrevious);
    connect(closeBtn, &QToolButton::clicked, this, [this]() {
        hide();
        emit closed();
    });
}

QString FindBar::text() const
{
    return m_edit->text();
}

void FindBar::focusInput()
{
    m_edit->setFocus();
    m_edit->selectAll();
}

void FindBar::setMatchInfo(int current, int total)
{
    if (text().isEmpty())
        m_matchLabel->clear();
    else if (total == 0)
        m_matchLabel->setText(tr("No results"));
    else
        m_matchLabel->setText(tr("%1 of %2").arg(current).arg(total));
}

void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit closed();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace ui
