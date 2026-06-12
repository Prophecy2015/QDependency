#include "ui/disasmdialog.h"

#include "core/disasm.h"
#include "core/peparser.h"

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ui {

namespace {

constexpr uint32_t kReadWindow = 64 * 1024;   // bytes read from the file per function

QString addrText(uint64_t addr, bool is64)
{
    return QStringLiteral("0x%1").arg(addr, is64 ? 16 : 8, 16, QLatin1Char('0'));
}

} // namespace

DisasmDialog::DisasmDialog(const core::PeInfoPtr &pe, const core::ExportEntry &exp,
                           QWidget *parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    const QString funcName = exp.name.isEmpty()
                                 ? QStringLiteral("Ordinal %1").arg(exp.ordinal)
                                 : exp.name;
    setWindowTitle(tr("Disassembly — %1").arg(funcName));
    resize(720, 560);

    auto *layout = new QVBoxLayout(this);

    auto *header = new QLabel(this);
    header->setObjectName(QStringLiteral("disasmHeader"));
    header->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *table = new QTableWidget(this);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({tr("Address"), tr("Bytes"), tr("Instruction")});
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->horizontalHeader()->setHighlightSections(false);
    QFont mono(QStringLiteral("Consolas"));
    mono.setPointSizeF(9.5);
    table->setFont(mono);
    layout->addWidget(header);
    layout->addWidget(table, 1);

    // forwarded exports and data exports have no code to disassemble
    core::DisasmResult dis;
    if (exp.isForwarded()) {
        dis.error = tr("This export is forwarded to %1 and has no local code.")
                        .arg(exp.forward);
    } else if (exp.rva == 0) {
        dis.error = tr("This export has no entry-point RVA.");
    } else {
        const QByteArray code =
            core::readImageBytesAtRva(pe->filePath, exp.rva, kReadWindow);
        dis = core::disassemble(code, pe->preferredBase + exp.rva, pe->is64);
    }

    header->setText(tr("%1   ·   %2   ·   entry 0x%3   ·   %4 instructions%5")
                        .arg(QFileInfo(pe->filePath).fileName(),
                             pe->is64 ? QStringLiteral("x64") : QStringLiteral("x86"))
                        .arg(exp.rva, 0, 16)
                        .arg(dis.instructions.size())
                        .arg(dis.truncated ? tr(" (truncated)") : QString()));

    if (!dis.error.isEmpty() && dis.instructions.isEmpty()) {
        table->setRowCount(1);
        table->setSpan(0, 0, 1, 3);
        auto *item = new QTableWidgetItem(dis.error);
        item->setForeground(QColor(0xcc, 0xa7, 0x00));
        table->setItem(0, 0, item);
    } else {
        table->setRowCount(dis.instructions.size());
        for (int row = 0; row < dis.instructions.size(); ++row) {
            const auto &ins = dis.instructions.at(row);
            auto *addr = new QTableWidgetItem(addrText(ins.address, pe->is64));
            addr->setForeground(QColor(0x85, 0x85, 0x85));
            auto *bytes = new QTableWidgetItem(ins.bytes);
            bytes->setForeground(QColor(0x85, 0x85, 0x85));
            const QString text = ins.operands.isEmpty()
                                     ? ins.mnemonic
                                     : ins.mnemonic + QLatin1String("   ") + ins.operands;
            auto *instr = new QTableWidgetItem(text);
            instr->setForeground(QColor(0xd4, 0xd4, 0xd4));
            table->setItem(row, 0, addr);
            table->setItem(row, 1, bytes);
            table->setItem(row, 2, instr);
        }
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *copyBtn = new QPushButton(tr("Copy"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    buttons->addWidget(copyBtn);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(copyBtn, &QPushButton::clicked, this, [this, dis, pe]() {
        QStringList lines;
        for (const auto &ins : dis.instructions) {
            lines.append(QStringLiteral("%1  %2  %3 %4")
                             .arg(addrText(ins.address, pe->is64), -18)
                             .arg(ins.bytes, -32)
                             .arg(ins.mnemonic, ins.operands));
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });
}

} // namespace ui
