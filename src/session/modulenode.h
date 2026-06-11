#pragma once

#include "core/peinfo.h"

#include <QList>
#include <QString>
#include <memory>
#include <vector>

namespace session {

enum class ModuleStatus { Ok, Missing, Invalid };

// one function that the parent module imports from this module
struct ImportRef {
    core::ImportFunc func;
    bool resolved = false;
    uint32_t entryRva = 0;
    QString forward;        // forward target of the resolved export
};

struct ModuleNode {
    QString rawName;        // name as referenced by the parent (or file name for root)
    QString resolvedPath;   // full path, empty when missing
    QString apiSetHost;     // host dll when rawName is an API set contract
    QString searchNote;     // which loader search step located the module
    core::PeInfoPtr pe;     // null when missing

    ModuleStatus status = ModuleStatus::Ok;
    bool delayLoad = false;
    bool forwarded = false;     // created from a forwarded export chain
    bool duplicate = false;     // module already expanded elsewhere, subtree pruned
    bool cpuMismatch = false;   // CPU differs from the root module
    bool hasMissingImports = false;
    bool subtreeHasIssue = false;   // any descendant missing / unresolved

    std::vector<ImportRef> parentImports;
    ModuleNode *parent = nullptr;
    std::vector<std::unique_ptr<ModuleNode>> children;

    QString moduleKey() const
    {
        return resolvedPath.isEmpty() ? QLatin1Char('?') + rawName.toLower()
                                      : resolvedPath.toLower();
    }
};

// unique module, one row in the module list pane
struct ModuleRecord {
    QString key;
    QString path;           // full path, or raw name when missing
    core::PeInfoPtr pe;
    ModuleStatus status = ModuleStatus::Ok;
    bool delayLoadOnly = true;
    bool cpuMismatch = false;
    bool hasMissingImports = false;
};

struct LogEntry {
    enum Level { Info = 0, Warning = 1, Error = 2 };
    int level = Info;
    QString text;
};

struct AnalysisResult {
    QString rootPath;
    std::unique_ptr<ModuleNode> root;
    std::vector<ModuleRecord> moduleList;
    QList<LogEntry> log;
    int errorCount = 0;
    int warningCount = 0;
    int moduleCount = 0;
};

using AnalysisResultPtr = std::shared_ptr<AnalysisResult>;

} // namespace session
