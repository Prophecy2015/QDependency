#pragma once

#include "core/peinfo.h"

#include <QDialog>

namespace ui {

// Modeless disassembly viewer for a single exported function.
class DisasmDialog : public QDialog {
    Q_OBJECT
public:
    // Disassembles `exp` from `pe` (uses pe->is64 to choose 32/64-bit mode).
    DisasmDialog(const core::PeInfoPtr &pe, const core::ExportEntry &exp,
                 QWidget *parent = nullptr);
};

} // namespace ui
