#pragma once

#include "core/peinfo.h"

namespace core {

// Parses a PE file from disk (read-only memory mapping, never loads it).
// Always returns a PeInfo; check ->valid / ->parseError.
std::shared_ptr<PeInfo> parsePeFile(const QString &filePath);

} // namespace core
