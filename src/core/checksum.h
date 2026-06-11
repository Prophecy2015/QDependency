#pragma once

#include <cstddef>
#include <cstdint>

namespace core {

// PE image checksum (same algorithm as imagehlp!CheckSumMappedFile).
// checksumFieldOffset: file offset of the OptionalHeader.CheckSum dword,
// which is excluded from the sum.
uint32_t computePeChecksum(const uint8_t *data, size_t size, size_t checksumFieldOffset);

} // namespace core
