#include "core/knowndlls.h"

#include <windows.h>

namespace core {

static QSet<QString> loadKnownDlls()
{
    QSet<QString> result;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\KnownDLLs",
                      0, KEY_READ, &key) != ERROR_SUCCESS)
        return result;

    for (DWORD i = 0;; ++i) {
        wchar_t valueName[256];
        DWORD nameLen = 256;
        wchar_t data[512];
        DWORD dataLen = sizeof(data);
        DWORD type = 0;
        const LSTATUS rc = RegEnumValueW(key, i, valueName, &nameLen, nullptr, &type,
                                         reinterpret_cast<BYTE *>(data), &dataLen);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS || type != REG_SZ)
            continue;
        const QString dll =
            QString::fromWCharArray(data, int(dataLen / sizeof(wchar_t))).trimmed()
                .remove(QLatin1Char('\0')).toLower();
        if (dll.endsWith(QLatin1String(".dll")))
            result.insert(dll);
    }
    RegCloseKey(key);

    // ntdll is always mapped by the loader even though it is not listed
    result.insert(QStringLiteral("ntdll.dll"));
    return result;
}

const QSet<QString> &knownDlls()
{
    static const QSet<QString> cached = loadKnownDlls();
    return cached;
}

} // namespace core
