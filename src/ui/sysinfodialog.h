#pragma once

#include <QDialog>

namespace ui {

// View > System Information, mirrors the depends.exe dialog.
class SysInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit SysInfoDialog(QWidget *parent = nullptr);

    static QString collectSystemInfo();
};

} // namespace ui
