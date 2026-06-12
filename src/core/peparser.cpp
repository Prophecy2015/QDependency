#include "core/peparser.h"
#include "core/checksum.h"

#include <QFile>
#include <QFileInfo>

#include <windows.h>
#include <winver.h>

#include <algorithm>
#include <vector>

namespace core {

QString cpuTypeName(CpuType cpu)
{
    switch (cpu) {
    case CpuType::X86:   return QStringLiteral("x86");
    case CpuType::X64:   return QStringLiteral("x64");
    case CpuType::Arm:   return QStringLiteral("ARM");
    case CpuType::Arm64: return QStringLiteral("ARM64");
    case CpuType::IA64:  return QStringLiteral("IA64");
    case CpuType::Other: return QStringLiteral("Other");
    default:             return QStringLiteral("Unknown");
    }
}

QString subsystemName(uint16_t subsystem)
{
    switch (subsystem) {
    case IMAGE_SUBSYSTEM_NATIVE:                  return QStringLiteral("Native");
    case IMAGE_SUBSYSTEM_WINDOWS_GUI:             return QStringLiteral("GUI");
    case IMAGE_SUBSYSTEM_WINDOWS_CUI:             return QStringLiteral("Console");
    case IMAGE_SUBSYSTEM_OS2_CUI:                 return QStringLiteral("OS/2");
    case IMAGE_SUBSYSTEM_POSIX_CUI:               return QStringLiteral("Posix");
    case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI:          return QStringLiteral("WinCE");
    case IMAGE_SUBSYSTEM_EFI_APPLICATION:         return QStringLiteral("EFI App");
    case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: return QStringLiteral("EFI Boot");
    case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:      return QStringLiteral("EFI Runtime");
    case IMAGE_SUBSYSTEM_EFI_ROM:                 return QStringLiteral("EFI ROM");
    case IMAGE_SUBSYSTEM_XBOX:                    return QStringLiteral("Xbox");
    default:                                      return QStringLiteral("Unknown");
    }
}

const ExportEntry *PeInfo::findExportByName(const QString &name) const
{
    const auto it = exportNameIndex.constFind(name);
    return it == exportNameIndex.constEnd() ? nullptr : &exports[*it];
}

const ExportEntry *PeInfo::findExportByOrdinal(uint32_t ordinal) const
{
    const auto it = exportOrdinalIndex.constFind(ordinal);
    return it == exportOrdinalIndex.constEnd() ? nullptr : &exports[*it];
}

const ExportEntry *PeInfo::findExport(const ImportFunc &f) const
{
    return f.byOrdinal ? findExportByOrdinal(f.ordinal) : findExportByName(f.name);
}

namespace {

constexpr size_t kNpos = static_cast<size_t>(-1);
constexpr uint32_t kMaxFuncs = 0x100000;        // sanity cap on table sizes
constexpr uint32_t kMaxDescriptors = 8192;

struct SectionRange {
    uint32_t va = 0;
    uint32_t vsize = 0;
    uint32_t raw = 0;
    uint32_t rawSize = 0;
};

class PeReader {
public:
    PeReader(const uint8_t *data, size_t size) : m_d(data), m_n(size) {}

    bool has(size_t off, size_t len) const { return off <= m_n && len <= m_n - off; }

    template <typename T>
    const T *ptr(size_t off) const
    {
        return has(off, sizeof(T)) ? reinterpret_cast<const T *>(m_d + off) : nullptr;
    }

    const uint8_t *raw(size_t off, size_t len) const
    {
        return has(off, len) ? m_d + off : nullptr;
    }

    QString cstr(size_t off, size_t maxLen = 4096) const
    {
        if (off >= m_n)
            return {};
        size_t end = off;
        const size_t limit = std::min(m_n, off + maxLen);
        while (end < limit && m_d[end] != 0)
            ++end;
        return QString::fromLatin1(reinterpret_cast<const char *>(m_d + off), int(end - off));
    }

    void setSections(std::vector<SectionRange> sections, uint32_t sizeOfHeaders)
    {
        m_sections = std::move(sections);
        m_sizeOfHeaders = sizeOfHeaders;
    }

    size_t rvaToOff(uint32_t rva) const
    {
        if (rva < m_sizeOfHeaders)
            return rva < m_n ? rva : kNpos;
        for (const auto &s : m_sections) {
            const uint32_t span = std::max(s.vsize, s.rawSize);
            if (rva >= s.va && rva - s.va < span) {
                if (rva - s.va >= s.rawSize)
                    return kNpos;   // data only present in memory, not on disk
                const size_t off = size_t(s.raw) + (rva - s.va);
                return off < m_n ? off : kNpos;
            }
        }
        return kNpos;
    }

    QString cstrAtRva(uint32_t rva, size_t maxLen = 4096) const
    {
        const size_t off = rvaToOff(rva);
        return off == kNpos ? QString() : cstr(off, maxLen);
    }

    size_t size() const { return m_n; }

private:
    const uint8_t *m_d;
    size_t m_n;
    std::vector<SectionRange> m_sections;
    uint32_t m_sizeOfHeaders = 0;
};

CpuType machineToCpu(uint16_t machine)
{
    switch (machine) {
    case IMAGE_FILE_MACHINE_I386:  return CpuType::X86;
    case IMAGE_FILE_MACHINE_AMD64: return CpuType::X64;
    case 0xAA64:                   return CpuType::Arm64;
    case IMAGE_FILE_MACHINE_ARM:
    case 0x01C4:                   return CpuType::Arm;
    case IMAGE_FILE_MACHINE_IA64:  return CpuType::IA64;
    default:                       return CpuType::Other;
    }
}

QString versionPair(uint16_t major, uint16_t minor)
{
    return QStringLiteral("%1.%2").arg(major).arg(minor);
}

void readFileMeta(PeInfo &info, const QString &path)
{
    const QFileInfo fi(path);
    info.fileSize = fi.size();
    info.fileTime = fi.lastModified();

    const DWORD attrs = GetFileAttributesW(reinterpret_cast<const wchar_t *>(path.utf16()));
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        QString s;
        if (attrs & FILE_ATTRIBUTE_READONLY)   s += QLatin1Char('R');
        if (attrs & FILE_ATTRIBUTE_HIDDEN)     s += QLatin1Char('H');
        if (attrs & FILE_ATTRIBUTE_SYSTEM)     s += QLatin1Char('S');
        if (attrs & FILE_ATTRIBUTE_ARCHIVE)    s += QLatin1Char('A');
        if (attrs & FILE_ATTRIBUTE_COMPRESSED) s += QLatin1Char('C');
        if (attrs & FILE_ATTRIBUTE_ENCRYPTED)  s += QLatin1Char('E');
        info.fileAttributes = s;
    }
}

void readVersionResource(PeInfo &info, const QString &path)
{
    const auto *wpath = reinterpret_cast<const wchar_t *>(path.utf16());
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(wpath, &handle);
    if (!size)
        return;
    std::vector<uint8_t> buf(size);
    if (!GetFileVersionInfoW(wpath, 0, size, buf.data()))
        return;
    VS_FIXEDFILEINFO *ffi = nullptr;
    UINT len = 0;
    if (VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void **>(&ffi), &len) && ffi
        && ffi->dwSignature == 0xFEEF04BD) {
        info.fileVer = QStringLiteral("%1.%2.%3.%4")
                           .arg(HIWORD(ffi->dwFileVersionMS)).arg(LOWORD(ffi->dwFileVersionMS))
                           .arg(HIWORD(ffi->dwFileVersionLS)).arg(LOWORD(ffi->dwFileVersionLS));
        info.productVer = QStringLiteral("%1.%2.%3.%4")
                              .arg(HIWORD(ffi->dwProductVersionMS)).arg(LOWORD(ffi->dwProductVersionMS))
                              .arg(HIWORD(ffi->dwProductVersionLS)).arg(LOWORD(ffi->dwProductVersionLS));
    }
}

void parseExports(PeInfo &info, const PeReader &r, uint32_t dirRva, uint32_t dirSize)
{
    const size_t off = r.rvaToOff(dirRva);
    const auto *dir = off == kNpos ? nullptr : r.ptr<IMAGE_EXPORT_DIRECTORY>(off);
    if (!dir)
        return;

    const uint32_t nFuncs = std::min<uint32_t>(dir->NumberOfFunctions, kMaxFuncs);
    const uint32_t nNames = std::min<uint32_t>(dir->NumberOfNames, kMaxFuncs);

    const size_t funcsOff = r.rvaToOff(dir->AddressOfFunctions);
    const size_t namesOff = r.rvaToOff(dir->AddressOfNames);
    const size_t ordsOff = r.rvaToOff(dir->AddressOfNameOrdinals);
    if (funcsOff == kNpos || !r.has(funcsOff, size_t(nFuncs) * 4))
        return;
    const auto *funcs = reinterpret_cast<const uint32_t *>(r.raw(funcsOff, size_t(nFuncs) * 4));

    std::vector<ExportEntry> expSlots(nFuncs);
    for (uint32_t i = 0; i < nFuncs; ++i) {
        expSlots[i].ordinal = dir->Base + i;
        expSlots[i].rva = funcs[i];
        if (funcs[i] >= dirRva && funcs[i] < dirRva + dirSize)
            expSlots[i].forward = r.cstrAtRva(funcs[i]);
    }

    if (namesOff != kNpos && ordsOff != kNpos
        && r.has(namesOff, size_t(nNames) * 4) && r.has(ordsOff, size_t(nNames) * 2)) {
        const auto *names = reinterpret_cast<const uint32_t *>(r.raw(namesOff, size_t(nNames) * 4));
        const auto *ords = reinterpret_cast<const uint16_t *>(r.raw(ordsOff, size_t(nNames) * 2));
        for (uint32_t j = 0; j < nNames; ++j) {
            const uint16_t idx = ords[j];
            if (idx < nFuncs) {
                expSlots[idx].name = r.cstrAtRva(names[j]);
                expSlots[idx].hint = int32_t(j);
            }
        }
    }

    info.exports.reserve(nFuncs);
    for (auto &e : expSlots) {
        if (e.rva == 0 && e.name.isEmpty())
            continue;   // unused slot
        const int idx = int(info.exports.size());
        if (!e.name.isEmpty())
            info.exportNameIndex.insert(e.name, idx);
        info.exportOrdinalIndex.insert(e.ordinal, idx);
        info.exports.push_back(std::move(e));
    }
}

void parseThunks(const PeReader &r, bool is64, uint32_t thunkRva,
                 std::vector<ImportFunc> &out)
{
    const size_t entrySize = is64 ? 8 : 4;
    for (uint32_t i = 0; i < kMaxFuncs; ++i) {
        const size_t off = r.rvaToOff(thunkRva + uint32_t(i * entrySize));
        if (off == kNpos)
            return;
        uint64_t val = 0;
        if (is64) {
            const auto *p = r.ptr<uint64_t>(off);
            if (!p)
                return;
            val = *p;
        } else {
            const auto *p = r.ptr<uint32_t>(off);
            if (!p)
                return;
            val = *p;
        }
        if (val == 0)
            return;

        ImportFunc f;
        const bool byOrdinal = is64 ? (val & (1ULL << 63)) != 0 : (val & 0x80000000u) != 0;
        if (byOrdinal) {
            f.byOrdinal = true;
            f.ordinal = uint32_t(val & 0xffff);
        } else {
            const uint32_t hintNameRva = uint32_t(val & 0x7fffffffu);
            const size_t hnOff = r.rvaToOff(hintNameRva);
            if (hnOff != kNpos) {
                const auto *hint = r.ptr<uint16_t>(hnOff);
                f.hint = hint ? int32_t(*hint) : -1;
                f.name = r.cstr(hnOff + 2);
            }
            if (f.name.isEmpty())
                continue;   // malformed entry, skip
        }
        out.push_back(std::move(f));
    }
}

void parseImports(PeInfo &info, const PeReader &r, uint32_t dirRva)
{
    for (uint32_t i = 0; i < kMaxDescriptors; ++i) {
        const size_t off = r.rvaToOff(dirRva + i * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        const auto *desc = off == kNpos ? nullptr : r.ptr<IMAGE_IMPORT_DESCRIPTOR>(off);
        if (!desc || (desc->Name == 0 && desc->FirstThunk == 0
                      && desc->OriginalFirstThunk == 0))
            return;

        ImportModule mod;
        mod.dllName = r.cstrAtRva(desc->Name);
        mod.bound = desc->TimeDateStamp != 0;
        const uint32_t thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk
                                                           : desc->FirstThunk;
        if (thunkRva)
            parseThunks(r, info.is64, thunkRva, mod.funcs);
        if (!mod.dllName.isEmpty())
            info.imports.push_back(std::move(mod));
    }
}

struct DelayDescriptor {
    uint32_t Attributes;
    uint32_t DllNameRVA;
    uint32_t ModuleHandleRVA;
    uint32_t ImportAddressTableRVA;
    uint32_t ImportNameTableRVA;
    uint32_t BoundImportAddressTableRVA;
    uint32_t UnloadInformationTableRVA;
    uint32_t TimeDateStamp;
};

void parseDelayImports(PeInfo &info, const PeReader &r, uint32_t dirRva)
{
    for (uint32_t i = 0; i < kMaxDescriptors; ++i) {
        const size_t off = r.rvaToOff(dirRva + i * uint32_t(sizeof(DelayDescriptor)));
        const auto *desc = off == kNpos ? nullptr : r.ptr<DelayDescriptor>(off);
        if (!desc || desc->DllNameRVA == 0)
            return;

        const bool rvaBased = (desc->Attributes & 1) != 0;
        const auto toRva = [&](uint32_t v) -> uint32_t {
            if (rvaBased || v == 0)
                return v;
            // legacy descriptors store VAs
            const uint64_t va = v;
            return va > info.preferredBase ? uint32_t(va - info.preferredBase) : 0;
        };

        ImportModule mod;
        mod.delayLoad = true;
        mod.dllName = r.cstrAtRva(toRva(desc->DllNameRVA));
        const uint32_t intRva = toRva(desc->ImportNameTableRVA);
        if (intRva)
            parseThunks(r, info.is64, intRva, mod.funcs);
        if (!mod.dllName.isEmpty())
            info.imports.push_back(std::move(mod));
    }
}

void parseManifestResource(PeInfo &info, const PeReader &r, uint32_t resRva)
{
    const size_t rootOff = r.rvaToOff(resRva);
    if (rootOff == kNpos)
        return;

    struct ResDirEntry {
        uint32_t name;
        uint32_t offset;
    };

    const auto firstChild = [&](size_t dirOff, uint32_t wantedId, bool anyId,
                                uint32_t *entryOffset) -> bool {
        const auto *dir = r.ptr<IMAGE_RESOURCE_DIRECTORY>(dirOff);
        if (!dir)
            return false;
        const uint32_t count = uint32_t(dir->NumberOfNamedEntries) + dir->NumberOfIdEntries;
        const size_t entriesOff = dirOff + sizeof(IMAGE_RESOURCE_DIRECTORY);
        for (uint32_t i = 0; i < std::min<uint32_t>(count, 4096); ++i) {
            const auto *e = r.ptr<ResDirEntry>(entriesOff + i * sizeof(ResDirEntry));
            if (!e)
                return false;
            const bool isId = (e->name & 0x80000000u) == 0;
            if (anyId || (isId && e->name == wantedId)) {
                *entryOffset = e->offset;
                return true;
            }
        }
        return false;
    };

    uint32_t typeEntry = 0;
    if (!firstChild(rootOff, 24 /* RT_MANIFEST */, false, &typeEntry)
        || !(typeEntry & 0x80000000u))
        return;
    uint32_t nameEntry = 0;
    if (!firstChild(rootOff + (typeEntry & 0x7fffffffu), 0, true, &nameEntry)
        || !(nameEntry & 0x80000000u))
        return;
    uint32_t langEntry = 0;
    if (!firstChild(rootOff + (nameEntry & 0x7fffffffu), 0, true, &langEntry)
        || (langEntry & 0x80000000u))
        return;

    const auto *data = r.ptr<IMAGE_RESOURCE_DATA_ENTRY>(rootOff + langEntry);
    if (!data || data->Size > 16 * 1024 * 1024)
        return;
    const size_t dataOff = r.rvaToOff(data->OffsetToData);
    const uint8_t *bytes = dataOff == kNpos ? nullptr : r.raw(dataOff, data->Size);
    if (bytes)
        info.manifestXml = QByteArray(reinterpret_cast<const char *>(bytes), int(data->Size));
}

void parseDebugDirectory(PeInfo &info, const PeReader &r, uint32_t dirRva, uint32_t dirSize)
{
    const uint32_t count = std::min<uint32_t>(dirSize / sizeof(IMAGE_DEBUG_DIRECTORY), 64);
    QStringList kinds;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t off = r.rvaToOff(dirRva + i * uint32_t(sizeof(IMAGE_DEBUG_DIRECTORY)));
        const auto *d = off == kNpos ? nullptr : r.ptr<IMAGE_DEBUG_DIRECTORY>(off);
        if (!d)
            break;
        QString kind;
        switch (d->Type) {
        case IMAGE_DEBUG_TYPE_COFF:     kind = QStringLiteral("COFF"); break;
        case IMAGE_DEBUG_TYPE_CODEVIEW: kind = QStringLiteral("CV"); break;
        case IMAGE_DEBUG_TYPE_FPO:      kind = QStringLiteral("FPO"); break;
        case IMAGE_DEBUG_TYPE_MISC:     kind = QStringLiteral("DBG"); break;
        case 13:                        kind = QStringLiteral("PGO"); break;
        case 16:                        kind = QStringLiteral("Repro"); break;
        default: break;
        }
        if (!kind.isEmpty() && !kinds.contains(kind))
            kinds.append(kind);
    }
    info.symbols = kinds.isEmpty() ? QStringLiteral("None") : kinds.join(QLatin1Char(','));
}

} // namespace

std::shared_ptr<PeInfo> parsePeFile(const QString &filePath)
{
    auto info = std::make_shared<PeInfo>();
    info->filePath = filePath;
    info->symbols = QStringLiteral("None");

    readFileMeta(*info, filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        info->parseError = QStringLiteral("Cannot open file: %1").arg(file.errorString());
        return info;
    }

    QByteArray fallback;
    const uchar *mapped = file.map(0, file.size());
    const uint8_t *data;
    size_t size;
    if (mapped) {
        data = mapped;
        size = size_t(file.size());
    } else {
        fallback = file.readAll();
        data = reinterpret_cast<const uint8_t *>(fallback.constData());
        size = size_t(fallback.size());
    }

    PeReader r(data, size);

    const auto *dos = r.ptr<IMAGE_DOS_HEADER>(0);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        info->parseError = QStringLiteral("Not a PE file (missing MZ signature).");
        return info;
    }
    const size_t ntOff = size_t(uint32_t(dos->e_lfanew));
    const auto *sig = r.ptr<uint32_t>(ntOff);
    if (!sig || *sig != IMAGE_NT_SIGNATURE) {
        info->parseError = QStringLiteral("Not a PE file (missing PE signature).");
        return info;
    }

    const size_t fhOff = ntOff + 4;
    const auto *fh = r.ptr<IMAGE_FILE_HEADER>(fhOff);
    if (!fh) {
        info->parseError = QStringLiteral("Truncated PE file header.");
        return info;
    }
    info->machine = fh->Machine;
    info->cpu = machineToCpu(fh->Machine);
    info->linkTimeStamp = fh->TimeDateStamp;
    info->characteristics = fh->Characteristics;

    const size_t ohOff = fhOff + sizeof(IMAGE_FILE_HEADER);
    const auto *magic = r.ptr<uint16_t>(ohOff);
    if (!magic) {
        info->parseError = QStringLiteral("Truncated optional header.");
        return info;
    }

    uint32_t numDirs = 0;
    IMAGE_DATA_DIRECTORY dirs[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {};
    uint32_t sizeOfHeaders = 0;

    if (*magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        const auto *oh = r.ptr<IMAGE_OPTIONAL_HEADER64>(ohOff);
        if (!oh) {
            info->parseError = QStringLiteral("Truncated optional header.");
            return info;
        }
        info->is64 = true;
        info->preferredBase = oh->ImageBase;
        info->sizeOfImage = oh->SizeOfImage;
        info->linkChecksum = oh->CheckSum;
        info->subsystem = oh->Subsystem;
        info->dllCharacteristics = oh->DllCharacteristics;
        info->linkerVer = versionPair(oh->MajorLinkerVersion, oh->MinorLinkerVersion);
        info->osVer = versionPair(oh->MajorOperatingSystemVersion, oh->MinorOperatingSystemVersion);
        info->imageVer = versionPair(oh->MajorImageVersion, oh->MinorImageVersion);
        info->subsystemVer = versionPair(oh->MajorSubsystemVersion, oh->MinorSubsystemVersion);
        sizeOfHeaders = oh->SizeOfHeaders;
        numDirs = std::min<uint32_t>(oh->NumberOfRvaAndSizes, IMAGE_NUMBEROF_DIRECTORY_ENTRIES);
        for (uint32_t i = 0; i < numDirs; ++i)
            dirs[i] = oh->DataDirectory[i];
    } else if (*magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        const auto *oh = r.ptr<IMAGE_OPTIONAL_HEADER32>(ohOff);
        if (!oh) {
            info->parseError = QStringLiteral("Truncated optional header.");
            return info;
        }
        info->is64 = false;
        info->preferredBase = oh->ImageBase;
        info->sizeOfImage = oh->SizeOfImage;
        info->linkChecksum = oh->CheckSum;
        info->subsystem = oh->Subsystem;
        info->dllCharacteristics = oh->DllCharacteristics;
        info->linkerVer = versionPair(oh->MajorLinkerVersion, oh->MinorLinkerVersion);
        info->osVer = versionPair(oh->MajorOperatingSystemVersion, oh->MinorOperatingSystemVersion);
        info->imageVer = versionPair(oh->MajorImageVersion, oh->MinorImageVersion);
        info->subsystemVer = versionPair(oh->MajorSubsystemVersion, oh->MinorSubsystemVersion);
        sizeOfHeaders = oh->SizeOfHeaders;
        numDirs = std::min<uint32_t>(oh->NumberOfRvaAndSizes, IMAGE_NUMBEROF_DIRECTORY_ENTRIES);
        for (uint32_t i = 0; i < numDirs; ++i)
            dirs[i] = oh->DataDirectory[i];
    } else {
        info->parseError = QStringLiteral("Unsupported optional header magic 0x%1.")
                               .arg(*magic, 0, 16);
        return info;
    }

    // section table
    const size_t secOff = ohOff + fh->SizeOfOptionalHeader;
    std::vector<SectionRange> sections;
    const uint32_t nSec = std::min<uint16_t>(fh->NumberOfSections, 384);
    sections.reserve(nSec);
    for (uint32_t i = 0; i < nSec; ++i) {
        const auto *s = r.ptr<IMAGE_SECTION_HEADER>(secOff + i * sizeof(IMAGE_SECTION_HEADER));
        if (!s)
            break;
        sections.push_back({s->VirtualAddress, s->Misc.VirtualSize,
                            s->PointerToRawData, s->SizeOfRawData});
    }
    r.setSections(std::move(sections), sizeOfHeaders);

    // checksum (the CheckSum field offset is identical for PE32 and PE32+)
    info->realChecksum = computePeChecksum(data, size, ohOff + 64);

    const auto dirAt = [&](uint32_t idx) -> IMAGE_DATA_DIRECTORY {
        return idx < numDirs ? dirs[idx] : IMAGE_DATA_DIRECTORY{};
    };

    if (const auto d = dirAt(IMAGE_DIRECTORY_ENTRY_EXPORT); d.VirtualAddress && d.Size)
        parseExports(*info, r, d.VirtualAddress, d.Size);
    if (const auto d = dirAt(IMAGE_DIRECTORY_ENTRY_IMPORT); d.VirtualAddress)
        parseImports(*info, r, d.VirtualAddress);
    if (const auto d = dirAt(IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT); d.VirtualAddress)
        parseDelayImports(*info, r, d.VirtualAddress);
    if (const auto d = dirAt(IMAGE_DIRECTORY_ENTRY_RESOURCE); d.VirtualAddress)
        parseManifestResource(*info, r, d.VirtualAddress);
    if (const auto d = dirAt(IMAGE_DIRECTORY_ENTRY_DEBUG); d.VirtualAddress && d.Size)
        parseDebugDirectory(*info, r, d.VirtualAddress, d.Size);
    info->hasTls = dirAt(IMAGE_DIRECTORY_ENTRY_TLS).VirtualAddress != 0;
    info->isDotNet = dirAt(IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR).VirtualAddress != 0;

    readVersionResource(*info, filePath);

    info->valid = true;
    return info;
}

QByteArray readImageBytesAtRva(const QString &filePath, uint32_t rva, uint32_t maxLen)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QByteArray fallback;
    const uchar *mapped = file.map(0, file.size());
    const uint8_t *data;
    size_t size;
    if (mapped) {
        data = mapped;
        size = size_t(file.size());
    } else {
        fallback = file.readAll();
        data = reinterpret_cast<const uint8_t *>(fallback.constData());
        size = size_t(fallback.size());
    }

    PeReader r(data, size);
    const auto *dos = r.ptr<IMAGE_DOS_HEADER>(0);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return {};
    const size_t ntOff = size_t(uint32_t(dos->e_lfanew));
    const auto *sig = r.ptr<uint32_t>(ntOff);
    if (!sig || *sig != IMAGE_NT_SIGNATURE)
        return {};
    const size_t fhOff = ntOff + 4;
    const auto *fh = r.ptr<IMAGE_FILE_HEADER>(fhOff);
    if (!fh)
        return {};
    const size_t ohOff = fhOff + sizeof(IMAGE_FILE_HEADER);
    const auto *magic = r.ptr<uint16_t>(ohOff);
    if (!magic)
        return {};
    const uint32_t sizeOfHeaders =
        (*magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            ? (r.ptr<IMAGE_OPTIONAL_HEADER64>(ohOff)
                   ? r.ptr<IMAGE_OPTIONAL_HEADER64>(ohOff)->SizeOfHeaders : 0)
            : (r.ptr<IMAGE_OPTIONAL_HEADER32>(ohOff)
                   ? r.ptr<IMAGE_OPTIONAL_HEADER32>(ohOff)->SizeOfHeaders : 0);

    const size_t secOff = ohOff + fh->SizeOfOptionalHeader;
    std::vector<SectionRange> sections;
    const uint32_t nSec = std::min<uint16_t>(fh->NumberOfSections, 384);
    sections.reserve(nSec);
    for (uint32_t i = 0; i < nSec; ++i) {
        const auto *s = r.ptr<IMAGE_SECTION_HEADER>(secOff + i * sizeof(IMAGE_SECTION_HEADER));
        if (!s)
            break;
        sections.push_back({s->VirtualAddress, s->Misc.VirtualSize,
                            s->PointerToRawData, s->SizeOfRawData});
    }
    r.setSections(std::move(sections), sizeOfHeaders);

    const size_t off = r.rvaToOff(rva);
    if (off == kNpos)
        return {};
    const size_t avail = size > off ? size - off : 0;
    const size_t len = std::min<size_t>(maxLen, avail);
    if (len == 0)
        return {};
    return QByteArray(reinterpret_cast<const char *>(data + off), int(len));
}

} // namespace core

