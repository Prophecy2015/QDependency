#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace core {

struct ResolvedModule {
    bool found = false;
    QString path;        // full path when found
    QString apiSetHost;  // host dll name when the request was an API set contract
    QString searchNote;  // which search-order step located the module
};

// Simulates the Windows loader DLL search order for static analysis.
class Resolver {
public:
    // appDir: directory of the root module being analyzed
    explicit Resolver(const QString &appDir);

    // registers SxS directories from an embedded manifest (best effort)
    void addManifest(const QByteArray &manifestXml, bool importer64);

    ResolvedModule resolve(const QString &dllName, bool importer64) const;

    QStringList searchDirs(bool importer64) const;

private:
    QString m_appDir;
    QString m_winDir;
    QString m_sys64;
    QString m_sysWow;
    QString m_sys16;
    QStringList m_pathDirs;
    QStringList m_sxsDirs64;
    QStringList m_sxsDirs32;
};

} // namespace core
