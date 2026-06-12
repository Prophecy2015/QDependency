#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>

namespace core {

struct Instruction {
    uint64_t address = 0;   // virtual address (preferredBase + rva)
    QString bytes;          // "48 89 5c 24 08"
    QString mnemonic;       // "mov"
    QString operands;       // "rbx, qword ptr [rsp + 8]"
};

struct DisasmResult {
    QList<Instruction> instructions;
    bool truncated = false;     // stopped at the byte/instruction cap, not a clear end
    QString error;              // set when disassembly could not start
};

// Linearly disassembles a function starting at `address` in 32- or 64-bit mode.
// Stops at a heuristic function end (ret followed by padding) or a safety cap.
DisasmResult disassemble(const QByteArray &code, uint64_t address, bool is64);

} // namespace core
