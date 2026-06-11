#include "core/apiset.h"

#include <windows.h>
#include <winternl.h>

namespace core {

namespace {

// ApiSetMap schema version 6 (Windows 10 1709+ / Windows 11)
struct ApiSetNamespace {
    ULONG Version;
    ULONG Size;
    ULONG Flags;
    ULONG Count;
    ULONG EntryOffset;
    ULONG HashOffset;
    ULONG HashFactor;
};

struct ApiSetNamespaceEntry {
    ULONG Flags;
    ULONG NameOffset;
    ULONG NameLength;     // bytes
    ULONG HashedLength;   // bytes, name truncated at the last hyphen
    ULONG ValueOffset;
    ULONG ValueCount;
};

struct ApiSetValueEntry {
    ULONG Flags;
    ULONG NameOffset;     // importer alias name (0/0 = default)
    ULONG NameLength;
    ULONG ValueOffset;    // host dll name
    ULONG ValueLength;
};

const void *apiSetMapFromPeb()
{
    const auto *peb = reinterpret_cast<const uint8_t *>(
        NtCurrentTeb()->ProcessEnvironmentBlock);
    // PEB64 +0x68 = ApiSetMap (stable since Windows 7)
    return *reinterpret_cast<const void *const *>(peb + 0x68);
}

// strips ".dll" and the trailing version group ("-l1-1-0" -> "-l1-1")
QString contractKey(QString name)
{
    name = name.toLower();
    if (name.endsWith(QLatin1String(".dll")))
        name.chop(4);
    const qsizetype dash = name.lastIndexOf(QLatin1Char('-'));
    if (dash > 0)
        name.truncate(dash);
    return name;
}

} // namespace

const ApiSetResolver &ApiSetResolver::instance()
{
    static const ApiSetResolver inst;
    return inst;
}

bool ApiSetResolver::looksLikeApiSet(const QString &dllName)
{
    return dllName.startsWith(QLatin1String("api-"), Qt::CaseInsensitive)
        || dllName.startsWith(QLatin1String("ext-"), Qt::CaseInsensitive);
}

QString ApiSetResolver::resolve(const QString &dllName) const
{
    return m_map.value(contractKey(dllName));
}

ApiSetResolver::ApiSetResolver()
{
    const auto *base = reinterpret_cast<const uint8_t *>(apiSetMapFromPeb());
    if (!base)
        return;
    const auto *ns = reinterpret_cast<const ApiSetNamespace *>(base);
    if (ns->Version != 6 || ns->Count == 0 || ns->Count > 100000)
        return;

    const auto *entries =
        reinterpret_cast<const ApiSetNamespaceEntry *>(base + ns->EntryOffset);
    for (ULONG i = 0; i < ns->Count; ++i) {
        const ApiSetNamespaceEntry &e = entries[i];
        if (e.NameLength == 0 || e.NameLength > 512)
            continue;
        const QString name = QString::fromWCharArray(
            reinterpret_cast<const wchar_t *>(base + e.NameOffset),
            int(e.NameLength / sizeof(wchar_t)));

        // default value entry has an empty alias name
        QString host;
        const auto *values =
            reinterpret_cast<const ApiSetValueEntry *>(base + e.ValueOffset);
        for (ULONG v = 0; v < e.ValueCount; ++v) {
            if (values[v].NameLength == 0 && values[v].ValueLength > 0
                && values[v].ValueLength <= 512) {
                host = QString::fromWCharArray(
                    reinterpret_cast<const wchar_t *>(base + values[v].ValueOffset),
                    int(values[v].ValueLength / sizeof(wchar_t)));
                break;
            }
        }
        if (!host.isEmpty())
            m_map.insert(contractKey(name), host.toLower());
    }
}

} // namespace core
