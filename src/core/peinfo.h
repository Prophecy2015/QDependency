#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <cstdint>
#include <memory>
#include <vector>

namespace core {

enum class CpuType { Unknown, X86, X64, Arm, Arm64, IA64, Other };

QString cpuTypeName(CpuType cpu);
QString subsystemName(uint16_t subsystem);

struct ExportEntry {
    uint32_t ordinal = 0;
    int32_t hint = -1;          // -1 when exported by ordinal only
    QString name;               // empty when exported by ordinal only
    uint32_t rva = 0;
    QString forward;            // "NTDLL.RtlAllocateHeap" when forwarded
    bool isForwarded() const { return !forward.isEmpty(); }
};

struct ImportFunc {
    bool byOrdinal = false;
    uint32_t ordinal = 0;
    int32_t hint = -1;
    QString name;
    QString displayName() const
    {
        return byOrdinal ? QStringLiteral("Ordinal %1 (0x%2)")
                               .arg(ordinal)
                               .arg(ordinal, 4, 16, QLatin1Char('0'))
                         : name;
    }
};

struct ImportModule {
    QString dllName;
    bool delayLoad = false;
    bool bound = false;         // bound import (TimeDateStamp set)
    std::vector<ImportFunc> funcs;
};

struct PeInfo {
    QString filePath;
    bool valid = false;
    QString parseError;

    // headers
    bool is64 = false;
    uint16_t machine = 0;
    CpuType cpu = CpuType::Unknown;
    uint32_t linkTimeStamp = 0;
    uint16_t subsystem = 0;
    uint16_t characteristics = 0;
    uint16_t dllCharacteristics = 0;
    uint64_t preferredBase = 0;
    uint32_t sizeOfImage = 0;
    uint32_t linkChecksum = 0;
    uint32_t realChecksum = 0;
    QString linkerVer;
    QString osVer;
    QString imageVer;
    QString subsystemVer;
    bool isDotNet = false;
    bool hasTls = false;
    QString symbols;            // "CV", "COFF", ... or "None"

    // filesystem / version resource
    qint64 fileSize = 0;
    QDateTime fileTime;
    QString fileAttributes;     // "A", "HRA", ...
    QString fileVer;
    QString productVer;

    QByteArray manifestXml;     // embedded RT_MANIFEST, empty if none

    std::vector<ExportEntry> exports;
    std::vector<ImportModule> imports;   // implicit + delay-load (flagged)

    // lookup indexes into `exports`, built by the parser
    QHash<QString, int> exportNameIndex;
    QHash<uint32_t, int> exportOrdinalIndex;

    bool isDll() const { return (characteristics & 0x2000) != 0; }

    // find the export satisfying an import; nullptr if unresolved
    const ExportEntry *findExport(const ImportFunc &f) const;
    const ExportEntry *findExportByName(const QString &name) const;
    const ExportEntry *findExportByOrdinal(uint32_t ordinal) const;
};

using PeInfoPtr = std::shared_ptr<const PeInfo>;

} // namespace core
