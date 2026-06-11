// Console verification tool for the core layer.
// Usage: pedump <pe-file> [--imports] [--exports] [--resolve]

#include "core/apiset.h"
#include "core/demangle.h"
#include "core/peparser.h"
#include "core/resolver.h"
#include "session/analysissession.h"
#include "session/sessionserializer.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        out << "usage: pedump <pe-file> [--imports] [--exports] [--resolve]\n";
        return 1;
    }
    const QString path = args.at(1);
    const bool showImports = args.contains(QLatin1String("--imports"));
    const bool showExports = args.contains(QLatin1String("--exports"));
    const bool doResolve = args.contains(QLatin1String("--resolve"));

    if (args.contains(QLatin1String("--roundtrip"))) {
        const auto result = session::analyzeFile(path);
        const QString tmp = QStringLiteral("roundtrip.qds");
        QString error;
        if (!session::saveSession(*result, tmp, &error)) {
            out << "SAVE FAILED: " << error << "\n";
            return 3;
        }
        const auto loaded = session::loadSession(tmp, &error);
        if (!loaded) {
            out << "LOAD FAILED: " << error << "\n";
            return 3;
        }
        const auto countNodes = [](const session::ModuleNode &root) {
            int n = 0;
            std::function<void(const session::ModuleNode &)> walk =
                [&](const session::ModuleNode &node) {
                    ++n;
                    for (const auto &c : node.children)
                        walk(*c);
                };
            walk(root);
            return n;
        };
        const int nodesA = result->root ? countNodes(*result->root) : 0;
        const int nodesB = loaded->root ? countNodes(*loaded->root) : 0;
        out << "saved:  modules=" << result->moduleCount << " nodes=" << nodesA
            << " log=" << result->log.size() << "\n";
        out << "loaded: modules=" << loaded->moduleCount << " nodes=" << nodesB
            << " log=" << loaded->log.size() << "\n";
        out << (result->moduleCount == loaded->moduleCount && nodesA == nodesB
                    && result->log.size() == loaded->log.size()
                ? "ROUNDTRIP OK" : "ROUNDTRIP MISMATCH")
            << "\n";
        return 0;
    }

    if (args.contains(QLatin1String("--tree"))) {
        const auto result = session::analyzeFile(path);
        std::function<void(const session::ModuleNode &, int)> walk =
            [&](const session::ModuleNode &node, int depth) {
                QString flags;
                if (node.delayLoad) flags += QLatin1String(" [delay]");
                if (node.forwarded) flags += QLatin1String(" [fwd]");
                if (node.duplicate) flags += QLatin1String(" [dup]");
                if (node.status == session::ModuleStatus::Missing) flags += QLatin1String(" [MISSING]");
                if (node.status == session::ModuleStatus::Invalid) flags += QLatin1String(" [INVALID]");
                if (node.hasMissingImports) {
                    flags += QLatin1String(" [unresolved-imports -> ");
                    flags += node.resolvedPath + QLatin1Char(']');
                    for (const auto &ref : node.parentImports) {
                        if (!ref.resolved) {
                            flags += QLatin1String(" miss:") + ref.func.displayName();
                            break;
                        }
                    }
                }
                out << QString(depth * 2, QLatin1Char(' ')) << node.rawName
                    << " (" << node.parentImports.size() << ")" << flags << "\n";
                for (const auto &c : node.children)
                    walk(*c, depth + 1);
            };
        if (result->root)
            walk(*result->root, 0);
        out << "\nmodules=" << result->moduleCount << " errors=" << result->errorCount
            << " warnings=" << result->warningCount << "\n";
        int unresolvedTotal = 0;
        std::function<void(const session::ModuleNode &)> count =
            [&](const session::ModuleNode &node) {
                for (const auto &ref : node.parentImports)
                    if (!ref.resolved && node.status == session::ModuleStatus::Ok)
                        ++unresolvedTotal;
                for (const auto &c : node.children)
                    count(*c);
            };
        if (result->root)
            count(*result->root);
        out << "unresolved-import-refs=" << unresolvedTotal << "\n";
        return 0;
    }

    const auto pe = core::parsePeFile(path);
    if (!pe->valid) {
        out << "PARSE ERROR: " << pe->parseError << "\n";
        return 2;
    }

    out << "File:           " << pe->filePath << "\n";
    out << "CPU:            " << core::cpuTypeName(pe->cpu)
        << (pe->is64 ? " (PE32+)" : " (PE32)") << "\n";
    out << "Subsystem:      " << core::subsystemName(pe->subsystem) << "\n";
    out << "Preferred base: 0x" << QString::number(pe->preferredBase, 16) << "\n";
    out << "Image size:     0x" << QString::number(pe->sizeOfImage, 16) << "\n";
    out << "Link checksum:  0x" << QString::number(pe->linkChecksum, 16) << "\n";
    out << "Real checksum:  0x" << QString::number(pe->realChecksum, 16)
        << (pe->linkChecksum == 0 || pe->linkChecksum == pe->realChecksum ? " (ok)" : " (MISMATCH)")
        << "\n";
    out << "Linker ver:     " << pe->linkerVer << "\n";
    out << "File ver:       " << pe->fileVer << "\n";
    out << "Symbols:        " << pe->symbols << "\n";
    out << "DotNet:         " << (pe->isDotNet ? "yes" : "no")
        << "   TLS: " << (pe->hasTls ? "yes" : "no")
        << "   Manifest: " << (pe->manifestXml.isEmpty() ? "no" : "yes") << "\n";
    out << "Imports:        " << pe->imports.size() << " modules\n";
    out << "Exports:        " << pe->exports.size() << " functions\n";

    core::Resolver resolver(QFileInfo(path).absolutePath());
    resolver.addManifest(pe->manifestXml, pe->is64);
    out << "ApiSet map:     "
        << (core::ApiSetResolver::instance().available() ? "available" : "NOT available")
        << " (" << core::ApiSetResolver::instance().entryCount() << " contracts)\n\n";

    for (const auto &mod : pe->imports) {
        out << (mod.delayLoad ? "  [delay] " : "  [imp]   ") << mod.dllName
            << "  (" << mod.funcs.size() << " funcs)";
        if (doResolve) {
            const auto res = resolver.resolve(mod.dllName, pe->is64);
            if (res.found) {
                out << "  -> " << res.path << "  [" << res.searchNote;
                if (!res.apiSetHost.isEmpty())
                    out << ", apiset->" << res.apiSetHost;
                out << "]";
            } else {
                out << "  -> NOT FOUND";
            }
        }
        out << "\n";
        if (showImports) {
            for (const auto &f : mod.funcs)
                out << "      " << f.displayName()
                    << "    | undecorated: " << core::demangleSymbol(f.name) << "\n";
        }
    }

    if (showExports) {
        out << "\nExports:\n";
        for (const auto &e : pe->exports) {
            out << "  #" << e.ordinal << "  hint=" << e.hint << "  "
                << (e.name.isEmpty() ? QStringLiteral("<ordinal only>") : e.name);
            if (e.isForwarded())
                out << "  -> forward: " << e.forward;
            else
                out << "  rva=0x" << QString::number(e.rva, 16);
            out << "\n";
        }
    }
    return 0;
}
