#pragma once

#include <QLabel>

namespace ui {

// VS Code style pane header: small uppercase gray label on a darker strip.
class PaneTitleBar : public QLabel {
    Q_OBJECT
public:
    explicit PaneTitleBar(const QString &title, QWidget *parent = nullptr);
    void setTitle(const QString &title);
};

} // namespace ui
