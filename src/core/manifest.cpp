#include "core/manifest.h"

#include <QDir>
#include <QXmlStreamReader>

#include <windows.h>

namespace core {

QList<SxsDependency> parseManifestDependencies(const QByteArray &manifestXml)
{
    QList<SxsDependency> result;
    if (manifestXml.isEmpty())
        return result;

    QXmlStreamReader xml(manifestXml);
    bool inDependentAssembly = false;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QLatin1String("dependentAssembly")) {
                inDependentAssembly = true;
            } else if (inDependentAssembly
                       && xml.name() == QLatin1String("assemblyIdentity")) {
                const auto attrs = xml.attributes();
                if (attrs.value(QLatin1String("type")) == QLatin1String("win32")) {
                    SxsDependency dep;
                    dep.name = attrs.value(QLatin1String("name")).toString();
                    dep.version = attrs.value(QLatin1String("version")).toString();
                    dep.arch = attrs.value(QLatin1String("processorArchitecture")).toString();
                    dep.token = attrs.value(QLatin1String("publicKeyToken")).toString();
                    if (!dep.name.isEmpty())
                        result.append(dep);
                }
            }
        } else if (token == QXmlStreamReader::EndElement
                   && xml.name() == QLatin1String("dependentAssembly")) {
            inDependentAssembly = false;
        }
    }
    return result;
}

QStringList sxsCandidateDirs(const SxsDependency &dep, bool importer64)
{
    wchar_t winDir[MAX_PATH];
    if (!GetWindowsDirectoryW(winDir, MAX_PATH))
        return {};
    const QDir sxs(QString::fromWCharArray(winDir) + QLatin1String("/WinSxS"));
    if (!sxs.exists())
        return {};

    const QString arch = (dep.arch.isEmpty() || dep.arch == QLatin1String("*"))
                             ? (importer64 ? QStringLiteral("amd64") : QStringLiteral("x86"))
                             : dep.arch.toLower();
    const QString pattern = QStringLiteral("%1_%2_%3_*")
                                .arg(arch, dep.name.toLower(), dep.token.toLower());

    QStringList dirs = sxs.entryList({pattern}, QDir::Dirs | QDir::NoDotAndDotDot,
                                     QDir::Name | QDir::Reversed);
    QStringList result;
    result.reserve(dirs.size());
    for (const QString &d : dirs)
        result.append(sxs.absoluteFilePath(d));
    return result;
}

} // namespace core
