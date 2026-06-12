#pragma once

#include "core/peinfo.h"

#include <QDialog>

namespace ui {

// Read-only summary of a module's PE header fields (tree right-click → Header Info).
class HeaderInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit HeaderInfoDialog(const core::PeInfoPtr &pe, QWidget *parent = nullptr);

    static QString formatHeaderInfo(const core::PeInfo &pe);
};

} // namespace ui
