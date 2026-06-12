#include "core/disasm.h"

#include <capstone/capstone.h>

#include <algorithm>

namespace core {

namespace {

constexpr uint32_t kMaxInstructions = 2048;
constexpr uint64_t kLocalWindow = 0x2000;   // a branch target within this many
                                            // bytes of the entry counts as "inside
                                            // this function"

QString toHexBytes(const uint8_t *bytes, size_t n)
{
    QString out;
    out.reserve(int(n) * 3);
    for (size_t i = 0; i < n; ++i) {
        if (i)
            out += QLatin1Char(' ');
        out += QString::number(bytes[i], 16).rightJustified(2, QLatin1Char('0'));
    }
    return out;
}

// Parses a bare immediate operand like "0x10098930"; false for register or
// memory operands ("eax", "dword ptr [0x...]").
bool parseImmTarget(const QString &operands, uint64_t &out)
{
    if (!operands.startsWith(QLatin1String("0x")))
        return false;
    bool ok = false;
    out = operands.toULongLong(&ok, 16);
    return ok;
}

} // namespace

DisasmResult disassemble(const QByteArray &code, uint64_t address, bool is64)
{
    DisasmResult result;
    if (code.isEmpty()) {
        result.error = QStringLiteral("No code bytes available at this address.");
        return result;
    }

    csh handle = 0;
    const cs_mode mode = is64 ? CS_MODE_64 : CS_MODE_32;
    if (cs_open(CS_ARCH_X86, mode, &handle) != CS_ERR_OK) {
        result.error = QStringLiteral("Failed to initialize the disassembler engine.");
        return result;
    }

    cs_insn *insn = cs_malloc(handle);
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(code.constData());
    size_t remaining = size_t(code.size());
    uint64_t pc = address;
    const uint64_t funcStart = address;
    uint64_t maxLocalTarget = 0;   // furthest in-function forward branch target
    bool stoppedCleanly = false;

    while (remaining > 0 && result.instructions.size() < int(kMaxInstructions)) {
        if (!cs_disasm_iter(handle, &ptr, &remaining, &pc, insn))
            break;   // invalid byte sequence — stop cleanly

        Instruction out;
        out.address = insn->address;
        out.bytes = toHexBytes(insn->bytes, insn->size);
        out.mnemonic = QString::fromLatin1(insn->mnemonic);
        out.operands = QString::fromLatin1(insn->op_str);
        result.instructions.append(out);

        const QString m = out.mnemonic;

        // Record forward branch targets that land inside this function so we don't
        // mistake an early ret/jmp (with a handler or loop body after it) for the end.
        if (m.startsWith(QLatin1Char('j'))) {
            uint64_t tgt = 0;
            if (parseImmTarget(out.operands, tgt) && tgt > insn->address
                && tgt <= funcStart + kLocalWindow)
                maxLocalTarget = std::max(maxLocalTarget, tgt);
        }

        // ret / unconditional jmp end a basic block. If nothing branches past the
        // next address, this is the function end (a far `jmp` is a tail call /
        // incremental-link thunk — its target is another function, not ours).
        const bool terminator =
            m == QLatin1String("ret") || m == QLatin1String("retf")
            || m == QLatin1String("iret") || m == QLatin1String("iretd")
            || m == QLatin1String("iretq") || m == QLatin1String("jmp");
        if (terminator && pc > maxLocalTarget) {
            stoppedCleanly = true;
            break;
        }
    }

    // trim any trailing int3/nop padding we swept past
    while (!result.instructions.isEmpty()) {
        const QString &m = result.instructions.last().mnemonic;
        if (m == QLatin1String("int3") || m == QLatin1String("nop"))
            result.instructions.removeLast();
        else
            break;
    }

    if (!stoppedCleanly
        && (result.instructions.size() >= int(kMaxInstructions) || remaining > 0))
        result.truncated = true;

    cs_free(insn, 1);
    cs_close(&handle);

    if (result.instructions.isEmpty())
        result.error = QStringLiteral("Could not decode any instruction at this address.");
    return result;
}

} // namespace core
