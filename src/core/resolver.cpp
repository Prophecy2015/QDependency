#include "core/resolver.h"
#include "core/apiset.h"
#include "core/knowndlls.h"
#include "core/manifest.h"

#include <QDir>
#include <QFileInfo>

#include <windows.h>

namespace core {

Resolver::Resolver(const QString &appDir)
    : m_appDir(QDir::cleanPath(appDir))
{
    wchar_t buf[MAX_PATH];
    if (GetWindowsDirectoryW(buf, MAX_PATH))
        m_winDir = QDir::cleanPath(QString::fromWCharArray(buf));
    m_sys64 = m_winDir + QLatin1String("/System32");
    m_sysWow = m_winDir + QLatin1String("/SysWOW64");
    m_sys16 = m_winDir + QLatin1String("/System");

    const QString path = qEnvironmentVariable("PATH");
    for (const QString &dir : path.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString clean = QDir::cleanPath(dir.trimmed());
        if (!clean.isEmpty() && !m_pathDirs.contains(clean, Qt::CaseInsensitive))
            m_pathDirs.append(clean);
    }
}

void Resolver::addManifest(const QByteArray &manifestXml, bool importer64)
{
    const auto deps = parseManifestDependencies(manifestXml);
    for (const auto &dep : deps) {
        const QStringList dirs = sxsCandidateDirs(dep, importer64);
        QStringList &target = importer64 ? m_sxsDirs64 : m_sxsDirs32;
        for (const QString &d : dirs) {
            if (!target.contains(d, Qt::CaseInsensitive))
                target.append(d);
        }
    }
}

QStringList Resolver::searchDirs(bool importer64) const
{
    QStringList dirs;
    dirs << (importer64 ? m_sxsDirs64 : m_sxsDirs32);
    dirs << m_appDir
         << (importer64 ? m_sys64 : m_sysWow)
         << m_sys16
         << m_winDir;
    dirs << m_pathDirs;
    return dirs;
}

ResolvedModule Resolver::resolve(const QString &dllName, bool importer64) const
{
    ResolvedModule result;

    QString name = dllName.trimmed();
    if (name.isEmpty())
        return result;

    // explicit path in the import name
    if (name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char('/'))) {
        const QFileInfo fi(name);
        if (fi.isAbsolute() && fi.exists()) {
            result.found = true;
            result.path = QDir::cleanPath(fi.absoluteFilePath());
            result.searchNote = QStringLiteral("Explicit path");
            return result;
        }
        name = fi.fileName();
    }

    // the loader appends ".dll" when the name has no extension
    if (!name.contains(QLatin1Char('.')))
        name += QLatin1String(".dll");

    // API set contract redirection
    if (ApiSetResolver::looksLikeApiSet(name)) {
        const QString host = ApiSetResolver::instance().resolve(name);
        if (!host.isEmpty()) {
            result.apiSetHost = host;
            name = host;
        }
    }

    const QString sysDir = importer64 ? m_sys64 : m_sysWow;
    const auto tryDir = [&](QString dir, const QString &note) -> bool {
        if (dir.isEmpty())
            return false;
        // WOW64 file system redirection: a 32-bit loader never sees the real
        // System32 — redirect any System32 candidate to SysWOW64
        if (!importer64 && dir.startsWith(m_sys64, Qt::CaseInsensitive)
            && (dir.size() == m_sys64.size() || dir.at(m_sys64.size()) == QLatin1Char('/')))
            dir = m_sysWow + dir.mid(m_sys64.size());
        const QString candidate = dir + QLatin1Char('/') + name;
        if (QFileInfo::exists(candidate)) {
            result.found = true;
            result.path = QDir::cleanPath(candidate);
            result.searchNote = note;
            return true;
        }
        return false;
    };

    // 1. side-by-side directories from manifests
    for (const QString &d : (importer64 ? m_sxsDirs64 : m_sxsDirs32)) {
        if (tryDir(d, QStringLiteral("Side-by-side")))
            return result;
    }

    // 2. KnownDLLs always map from the system directory
    if (knownDlls().contains(name.toLower()) && tryDir(sysDir, QStringLiteral("KnownDLLs")))
        return result;

    // 3. application directory
    if (tryDir(m_appDir, QStringLiteral("Application directory")))
        return result;

    // 4. system directory (System32 / SysWOW64 by importer bitness)
    if (tryDir(sysDir, QStringLiteral("System directory")))
        return result;

    // 5. 16-bit system directory, 6. Windows directory
    if (tryDir(m_sys16, QStringLiteral("16-bit system directory")))
        return result;
    if (tryDir(m_winDir, QStringLiteral("Windows directory")))
        return result;

    // 7. PATH
    for (const QString &d : m_pathDirs) {
        if (tryDir(d, QStringLiteral("PATH")))
            return result;
    }

    return result;
}

} // namespace core
