#pragma once

#include <QWidget>

class QLineEdit;
class QToolButton;
class QLabel;

namespace ui {

// VS Code style inline find bar (Ctrl+F). Filters the active function pane
// and jumps between matches in the dependency tree.
class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);

    QString text() const;
    void focusInput();
    void setMatchInfo(int current, int total);

signals:
    void searchChanged(const QString &text);
    void findNext();
    void findPrevious();
    void closed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLineEdit *m_edit;
    QLabel *m_matchLabel;
};

} // namespace ui
