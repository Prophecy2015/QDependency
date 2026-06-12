#pragma once

#include "core/peinfo.h"

namespace core {

// Parses a PE file from disk (read-only memory mapping, never loads it).
// Always returns a PeInfo; check ->valid / ->parseError.
std::shared_ptr<PeInfo> parsePeFile(const QString &filePath);

// Reads up to maxLen raw bytes starting at an image RVA, mapping the RVA to a
// file offset via the section table (the bytes are not relocated). Used to feed
// a function's code to the disassembler on demand. Returns empty on failure.
QByteArray readImageBytesAtRva(const QString &filePath, uint32_t rva, uint32_t maxLen);

} // namespace core
