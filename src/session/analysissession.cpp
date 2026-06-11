#include "session/analysissession.h"

#include "core/peparser.h"
#include "core/resolver.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QThread>

namespace session {

namespace {

constexpr int kMaxDepth = 120;

class Analyzer {
public:
    explicit Analyzer(const std::function<void(QString)> &progress)
        : m_progress(progress) {}

    AnalysisResultPtr run(const QString &filePath)
    {
        auto result = std::make_shared<AnalysisResult>();
        const QFileInfo fi(filePath);
        result->rootPath = QDir::cleanPath(fi.absoluteFilePath());
        m_result = result.get();

        m_resolver = std::make_unique<core::Resolver>(fi.absolutePath());

        auto root = std::make_unique<ModuleNode>();
        root->rawName = fi.fileName();
        root->resolvedPath = result->rootPath;
        root->pe = parseCached(result->rootPath);

        if (!root->pe->valid) {
            root->status = ModuleStatus::Invalid;
            log(LogEntry::Error,
                QStringLiteral("Error: \"%1\" is not a valid PE file: %2")
                    .arg(root->rawName, root->pe->parseError));
        } else {
            m_rootCpu = root->pe->cpu;
            if (!root->pe->manifestXml.isEmpty())
                m_resolver->addManifest(root->pe->manifestXml, root->pe->is64);
            log(LogEntry::Info,
                QStringLiteral("Analyzing \"%1\" (%2, %3)...")
                    .arg(result->rootPath, core::cpuTypeName(root->pe->cpu),
                         root->pe->is64 ? QStringLiteral("PE32+") : QStringLiteral("PE32")));
            if (root->pe->isDotNet)
                log(LogEntry::Warning,
                    QStringLiteral("Warning: \"%1\" is a managed (.NET) assembly. Only its "
                                   "native import table is analyzed.").arg(root->rawName));
        }

        record(*root);
        if (root->pe->valid)
            expand(*root, 0);

        finalizeSummary(*result);
        result->root = std::move(root);
        result->moduleCount = int(result->moduleList.size());
        return result;
    }

private:
    void log(int level, const QString &text)
    {
        m_result->log.append({level, text});
        if (level == LogEntry::Error)
            ++m_result->errorCount;
        else if (level == LogEntry::Warning)
            ++m_result->warningCount;
    }

    core::PeInfoPtr parseCached(const QString &path)
    {
        const QString key = QDir::cleanPath(path).toLower();
        const auto it = m_peCache.constFind(key);
        if (it != m_peCache.constEnd())
            return *it;
        if (m_progress)
            m_progress(QFileInfo(path).fileName());
        core::PeInfoPtr pe = core::parsePeFile(QDir::cleanPath(path));
        m_peCache.insert(key, pe);
        return pe;
    }

    void record(const ModuleNode &node)
    {
        const QString key = node.moduleKey();
        const auto it = m_recordIndex.constFind(key);
        if (it != m_recordIndex.constEnd()) {
            ModuleRecord &rec = m_result->moduleList[*it];
            if (!node.delayLoad)
                rec.delayLoadOnly = false;
            rec.hasMissingImports = rec.hasMissingImports || node.hasMissingImports;
            return;
        }
        ModuleRecord rec;
        rec.key = key;
        rec.path = node.resolvedPath.isEmpty() ? node.rawName : node.resolvedPath;
        rec.pe = node.pe;
        rec.status = node.status;
        rec.delayLoadOnly = node.delayLoad;
        rec.cpuMismatch = node.cpuMismatch;
        rec.hasMissingImports = node.hasMissingImports;
        m_recordIndex.insert(key, int(m_result->moduleList.size()));
        m_result->moduleList.push_back(std::move(rec));
    }

    // resolves one referenced module name into a child node (no expansion yet)
    std::unique_ptr<ModuleNode> makeChild(ModuleNode &parent, const QString &dllName,
                                          bool delayLoad, bool forwarded)
    {
        auto child = std::make_unique<ModuleNode>();
        child->rawName = dllName;
        child->delayLoad = delayLoad;
        child->forwarded = forwarded;
        child->parent = &parent;

        const bool importer64 = parent.pe && parent.pe->is64;
        const auto res = m_resolver->resolve(dllName, importer64);
        child->apiSetHost = res.apiSetHost;
        child->searchNote = res.searchNote;

        if (!res.found) {
            child->status = ModuleStatus::Missing;
            // depends.exe treats missing delay-load modules as warnings, missing
            // implicit modules as errors
            if (delayLoad)
                log(LogEntry::Warning,
                    QStringLiteral("Warning: Delay-load dependency module \"%1\" "
                                   "(referenced by \"%2\") was not found.")
                        .arg(dllName, parent.rawName));
            else
                log(LogEntry::Error,
                    QStringLiteral("Error: Module \"%1\" (referenced by \"%2\") was not "
                                   "found.").arg(dllName, parent.rawName));
            return child;
        }

        child->resolvedPath = res.path;
        child->pe = parseCached(res.path);
        if (!child->pe->valid) {
            child->status = ModuleStatus::Invalid;
            log(LogEntry::Error,
                QStringLiteral("Error: Module \"%1\" is not a valid PE file: %2")
                    .arg(res.path, child->pe->parseError));
            return child;
        }
        if (m_rootCpu != core::CpuType::Unknown && child->pe->cpu != m_rootCpu) {
            child->cpuMismatch = true;
            log(LogEntry::Error,
                QStringLiteral("Error: Module \"%1\" (%2) has a different CPU type than the "
                               "root module (%3).")
                    .arg(child->rawName, core::cpuTypeName(child->pe->cpu),
                         core::cpuTypeName(m_rootCpu)));
        }
        if (!res.apiSetHost.isEmpty()) {
            const QString key = dllName.toLower();
            if (!m_loggedApiSets.contains(key)) {
                m_loggedApiSets.insert(key);
                log(LogEntry::Info,
                    QStringLiteral("API set \"%1\" redirected to \"%2\".")
                        .arg(dllName, res.apiSetHost));
            }
        }
        return child;
    }

    // matches the parent's import list against the child's exports
    void matchImports(ModuleNode &parent, ModuleNode &child,
                      const std::vector<core::ImportFunc> &funcs)
    {
        for (const auto &f : funcs) {
            ImportRef ref;
            ref.func = f;
            if (child.pe && child.pe->valid) {
                if (const auto *e = child.pe->findExport(f)) {
                    ref.resolved = true;
                    ref.entryRva = e->rva;
                    ref.forward = e->forward;
                } else {
                    child.hasMissingImports = true;
                    log(LogEntry::Error,
                        QStringLiteral("Error: Function \"%1\" was not found in \"%2\" "
                                       "(imported by \"%3\").")
                            .arg(f.displayName(),
                                 child.resolvedPath.isEmpty() ? child.rawName
                                                              : child.resolvedPath,
                                 parent.rawName));
                }
            }
            child.parentImports.push_back(std::move(ref));
        }
    }

    // creates child nodes for forwarded export targets actually used by the parent
    void addForwardTargets(ModuleNode &node, int depth)
    {
        struct FwdGroup {
            QString module;
            std::vector<core::ImportFunc> funcs;
        };
        std::vector<FwdGroup> groups;
        for (const auto &ref : node.parentImports) {
            if (ref.forward.isEmpty())
                continue;
            const qsizetype dot = ref.forward.lastIndexOf(QLatin1Char('.'));
            if (dot <= 0)
                continue;
            QString module = ref.forward.left(dot) + QLatin1String(".dll");
            QString target = ref.forward.mid(dot + 1);

            core::ImportFunc f;
            if (target.startsWith(QLatin1Char('#'))) {
                f.byOrdinal = true;
                f.ordinal = target.mid(1).toUInt();
            } else {
                f.name = target;
            }
            auto it = std::find_if(groups.begin(), groups.end(), [&](const FwdGroup &g) {
                return g.module.compare(module, Qt::CaseInsensitive) == 0;
            });
            if (it == groups.end())
                groups.push_back({module, {std::move(f)}});
            else
                it->funcs.push_back(std::move(f));
        }

        for (auto &g : groups) {
            // skip self-forwards
            if (node.rawName.compare(g.module, Qt::CaseInsensitive) == 0)
                continue;
            auto child = makeChild(node, g.module, node.delayLoad, true);
            matchImports(node, *child, g.funcs);
            record(*child);
            expand(*child, depth + 1);
            node.children.push_back(std::move(child));
        }
    }

    void expand(ModuleNode &node, int depth)
    {
        if (!node.pe || !node.pe->valid || depth > kMaxDepth)
            return;
        const QString key = node.moduleKey();
        if (m_expanded.contains(key)) {
            node.duplicate = true;
            return;
        }
        m_expanded.insert(key);

        if (node.pe->isDotNet && depth > 0)
            log(LogEntry::Warning,
                QStringLiteral("Warning: \"%1\" is a managed (.NET) assembly. Only its "
                               "native import table is analyzed.").arg(node.rawName));

        for (const auto &mod : node.pe->imports) {
            auto child = makeChild(node, mod.dllName, mod.delayLoad, false);
            matchImports(node, *child, mod.funcs);
            record(*child);
            expand(*child, depth + 1);
            node.children.push_back(std::move(child));
        }

        addForwardTargets(node, depth);
    }

    static bool computeIssues(ModuleNode &node)
    {
        bool issue = node.status != ModuleStatus::Ok || node.hasMissingImports
                     || node.cpuMismatch;
        for (auto &c : node.children)
            issue = computeIssues(*c) || issue;
        node.subtreeHasIssue = issue;
        return issue;
    }

    void finalizeSummary(AnalysisResult &result)
    {
        bool anyMissing = false;
        bool anyUnresolved = false;
        bool anyDelayMissing = false;
        for (const auto &rec : result.moduleList) {
            if (rec.status == ModuleStatus::Missing) {
                (rec.delayLoadOnly ? anyDelayMissing : anyMissing) = true;
            }
            if (rec.hasMissingImports)
                anyUnresolved = true;
        }
        if (anyMissing)
            log(LogEntry::Error,
                QStringLiteral("Error: At least one required implicit or forwarded "
                               "dependency module was not found."));
        if (anyDelayMissing)
            log(LogEntry::Warning,
                QStringLiteral("Warning: At least one delay-load dependency module was "
                               "not found."));
        if (anyUnresolved)
            log(LogEntry::Error,
                QStringLiteral("Error: At least one module has an unresolved import due to "
                               "a missing export function in a dependent module."));
    }

public:
    void postProcess(AnalysisResult &result)
    {
        if (result.root)
            computeIssues(*result.root);
    }

private:
    const std::function<void(QString)> &m_progress;
    AnalysisResult *m_result = nullptr;
    std::unique_ptr<core::Resolver> m_resolver;
    core::CpuType m_rootCpu = core::CpuType::Unknown;
    QHash<QString, core::PeInfoPtr> m_peCache;
    QHash<QString, int> m_recordIndex;
    QSet<QString> m_expanded;
    QSet<QString> m_loggedApiSets;
};

} // namespace

AnalysisResultPtr analyzeFile(const QString &filePath,
                              const std::function<void(QString)> &progress)
{
    Analyzer analyzer(progress);
    auto result = analyzer.run(filePath);
    analyzer.postProcess(*result);
    return result;
}

AnalysisSession::AnalysisSession(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<AnalysisResultPtr>("session::AnalysisResultPtr");
}

AnalysisSession::~AnalysisSession() = default;

void AnalysisSession::start(const QString &filePath)
{
    if (m_running)
        return;
    m_running = true;

    QThread *thread = QThread::create([this, filePath]() {
        const std::function<void(QString)> progress = [this](const QString &name) {
            emit progressText(name);
        };
        AnalysisResultPtr result = analyzeFile(filePath, progress);
        m_running = false;
        emit finished(result);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

} // namespace session
