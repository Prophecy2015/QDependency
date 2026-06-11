#include "core/checksum.h"

namespace core {

uint32_t computePeChecksum(const uint8_t *data, size_t size, size_t checksumFieldOffset)
{
    uint64_t sum = 0;
    size_t i = 0;
    for (; i + 1 < size; i += 2) {
        if (i >= checksumFieldOffset && i < checksumFieldOffset + 4)
            continue;
        sum += static_cast<uint16_t>(data[i] | (data[i + 1] << 8));
        sum = (sum & 0xffff) + (sum >> 16);
    }
    if (i < size) {
        sum += data[i];
        sum = (sum & 0xffff) + (sum >> 16);
    }
    sum = (sum & 0xffff) + (sum >> 16);
    return static_cast<uint32_t>(sum + size);
}

} // namespace core
