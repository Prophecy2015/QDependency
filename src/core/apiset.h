#pragma once

#include <QHash>
#include <QString>

namespace core {

// Resolves "api-ms-win-*" / "ext-ms-*" virtual DLL names to their host DLL
// using the ApiSetMap of the running system (read from this process' PEB).
class ApiSetResolver {
public:
    static const ApiSetResolver &instance();

    bool available() const { return !m_map.isEmpty(); }
    int entryCount() const { return int(m_map.size()); }

    // True if the name looks like an API set contract name.
    static bool looksLikeApiSet(const QString &dllName);

    // Returns the host DLL name (e.g. "kernelbase.dll"); empty if unknown.
    QString resolve(const QString &dllName) const;

private:
    ApiSetResolver();
    QHash<QString, QString> m_map;   // contract prefix (lowercase, no version) -> host dll
};

} // namespace core
