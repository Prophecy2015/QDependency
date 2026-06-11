#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace core {

struct SxsDependency {
    QString name;        // e.g. "Microsoft.Windows.Common-Controls"
    QString version;     // e.g. "6.0.0.0"
    QString arch;        // "x86", "amd64" or "*"
    QString token;       // publicKeyToken
};

// Extracts <dependentAssembly><assemblyIdentity> entries from a manifest.
QList<SxsDependency> parseManifestDependencies(const QByteArray &manifestXml);

// Best-effort: directories under C:\Windows\WinSxS matching the dependency,
// newest version first.
QStringList sxsCandidateDirs(const SxsDependency &dep, bool importer64);

} // namespace core
