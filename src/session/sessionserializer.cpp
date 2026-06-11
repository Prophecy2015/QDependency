#include "session/sessionserializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace session {

namespace {

QJsonObject peToJson(const core::PeInfo &pe)
{
    QJsonObject o;
    o[QLatin1String("filePath")] = pe.filePath;
    o[QLatin1String("valid")] = pe.valid;
    o[QLatin1String("parseError")] = pe.parseError;
    o[QLatin1String("is64")] = pe.is64;
    o[QLatin1String("machine")] = int(pe.machine);
    o[QLatin1String("cpu")] = int(pe.cpu);
    o[QLatin1String("linkTimeStamp")] = qint64(pe.linkTimeStamp);
    o[QLatin1String("subsystem")] = int(pe.subsystem);
    o[QLatin1String("characteristics")] = int(pe.characteristics);
    o[QLatin1String("preferredBase")] = QString::number(pe.preferredBase, 16);
    o[QLatin1String("sizeOfImage")] = qint64(pe.sizeOfImage);
    o[QLatin1String("linkChecksum")] = qint64(pe.linkChecksum);
    o[QLatin1String("realChecksum")] = qint64(pe.realChecksum);
    o[QLatin1String("linkerVer")] = pe.linkerVer;
    o[QLatin1String("osVer")] = pe.osVer;
    o[QLatin1String("imageVer")] = pe.imageVer;
    o[QLatin1String("subsystemVer")] = pe.subsystemVer;
    o[QLatin1String("isDotNet")] = pe.isDotNet;
    o[QLatin1String("hasTls")] = pe.hasTls;
    o[QLatin1String("symbols")] = pe.symbols;
    o[QLatin1String("fileSize")] = pe.fileSize;
    o[QLatin1String("fileTime")] = pe.fileTime.toString(Qt::ISODate);
    o[QLatin1String("fileAttributes")] = pe.fileAttributes;
    o[QLatin1String("fileVer")] = pe.fileVer;
    o[QLatin1String("productVer")] = pe.productVer;

    QJsonArray exports;
    for (const auto &e : pe.exports) {
        QJsonArray row{qint64(e.ordinal), e.hint, e.name, qint64(e.rva), e.forward};
        exports.append(row);
    }
    o[QLatin1String("exports")] = exports;
    return o;
}

core::PeInfoPtr peFromJson(const QJsonObject &o)
{
    auto pe = std::make_shared<core::PeInfo>();
    pe->filePath = o[QLatin1String("filePath")].toString();
    pe->valid = o[QLatin1String("valid")].toBool();
    pe->parseError = o[QLatin1String("parseError")].toString();
    pe->is64 = o[QLatin1String("is64")].toBool();
    pe->machine = uint16_t(o[QLatin1String("machine")].toInt());
    pe->cpu = core::CpuType(o[QLatin1String("cpu")].toInt());
    pe->linkTimeStamp = uint32_t(o[QLatin1String("linkTimeStamp")].toInteger());
    pe->subsystem = uint16_t(o[QLatin1String("subsystem")].toInt());
    pe->characteristics = uint16_t(o[QLatin1String("characteristics")].toInt());
    pe->preferredBase = o[QLatin1String("preferredBase")].toString().toULongLong(nullptr, 16);
    pe->sizeOfImage = uint32_t(o[QLatin1String("sizeOfImage")].toInteger());
    pe->linkChecksum = uint32_t(o[QLatin1String("linkChecksum")].toInteger());
    pe->realChecksum = uint32_t(o[QLatin1String("realChecksum")].toInteger());
    pe->linkerVer = o[QLatin1String("linkerVer")].toString();
    pe->osVer = o[QLatin1String("osVer")].toString();
    pe->imageVer = o[QLatin1String("imageVer")].toString();
    pe->subsystemVer = o[QLatin1String("subsystemVer")].toString();
    pe->isDotNet = o[QLatin1String("isDotNet")].toBool();
    pe->hasTls = o[QLatin1String("hasTls")].toBool();
    pe->symbols = o[QLatin1String("symbols")].toString();
    pe->fileSize = o[QLatin1String("fileSize")].toInteger();
    pe->fileTime = QDateTime::fromString(o[QLatin1String("fileTime")].toString(), Qt::ISODate);
    pe->fileAttributes = o[QLatin1String("fileAttributes")].toString();
    pe->fileVer = o[QLatin1String("fileVer")].toString();
    pe->productVer = o[QLatin1String("productVer")].toString();

    const QJsonArray exports = o[QLatin1String("exports")].toArray();
    pe->exports.reserve(exports.size());
    for (const auto &v : exports) {
        const QJsonArray row = v.toArray();
        core::ExportEntry e;
        e.ordinal = uint32_t(row.at(0).toInteger());
        e.hint = int32_t(row.at(1).toInt());
        e.name = row.at(2).toString();
        e.rva = uint32_t(row.at(3).toInteger());
        e.forward = row.at(4).toString();
        const int idx = int(pe->exports.size());
        if (!e.name.isEmpty())
            pe->exportNameIndex.insert(e.name, idx);
        pe->exportOrdinalIndex.insert(e.ordinal, idx);
        pe->exports.push_back(std::move(e));
    }
    return pe;
}

QJsonObject nodeToJson(const ModuleNode &node)
{
    QJsonObject o;
    o[QLatin1String("rawName")] = node.rawName;
    o[QLatin1String("key")] = node.moduleKey();
    o[QLatin1String("resolvedPath")] = node.resolvedPath;
    o[QLatin1String("apiSetHost")] = node.apiSetHost;
    o[QLatin1String("searchNote")] = node.searchNote;
    o[QLatin1String("status")] = int(node.status);
    o[QLatin1String("delayLoad")] = node.delayLoad;
    o[QLatin1String("forwarded")] = node.forwarded;
    o[QLatin1String("duplicate")] = node.duplicate;
    o[QLatin1String("cpuMismatch")] = node.cpuMismatch;
    o[QLatin1String("hasMissingImports")] = node.hasMissingImports;
    o[QLatin1String("subtreeHasIssue")] = node.subtreeHasIssue;

    QJsonArray imports;
    for (const auto &ref : node.parentImports) {
        QJsonArray row{ref.func.byOrdinal, qint64(ref.func.ordinal), ref.func.hint,
                       ref.func.name, ref.resolved, qint64(ref.entryRva), ref.forward};
        imports.append(row);
    }
    o[QLatin1String("parentImports")] = imports;

    QJsonArray children;
    for (const auto &c : node.children)
        children.append(nodeToJson(*c));
    o[QLatin1String("children")] = children;
    return o;
}

std::unique_ptr<ModuleNode> nodeFromJson(const QJsonObject &o,
                                         const QHash<QString, core::PeInfoPtr> &peByKey,
                                         ModuleNode *parent)
{
    auto node = std::make_unique<ModuleNode>();
    node->parent = parent;
    node->rawName = o[QLatin1String("rawName")].toString();
    node->resolvedPath = o[QLatin1String("resolvedPath")].toString();
    node->apiSetHost = o[QLatin1String("apiSetHost")].toString();
    node->searchNote = o[QLatin1String("searchNote")].toString();
    node->status = ModuleStatus(o[QLatin1String("status")].toInt());
    node->delayLoad = o[QLatin1String("delayLoad")].toBool();
    node->forwarded = o[QLatin1String("forwarded")].toBool();
    node->duplicate = o[QLatin1String("duplicate")].toBool();
    node->cpuMismatch = o[QLatin1String("cpuMismatch")].toBool();
    node->hasMissingImports = o[QLatin1String("hasMissingImports")].toBool();
    node->subtreeHasIssue = o[QLatin1String("subtreeHasIssue")].toBool();
    node->pe = peByKey.value(o[QLatin1String("key")].toString());

    for (const auto &v : o[QLatin1String("parentImports")].toArray()) {
        const QJsonArray row = v.toArray();
        ImportRef ref;
        ref.func.byOrdinal = row.at(0).toBool();
        ref.func.ordinal = uint32_t(row.at(1).toInteger());
        ref.func.hint = int32_t(row.at(2).toInt());
        ref.func.name = row.at(3).toString();
        ref.resolved = row.at(4).toBool();
        ref.entryRva = uint32_t(row.at(5).toInteger());
        ref.forward = row.at(6).toString();
        node->parentImports.push_back(std::move(ref));
    }

    for (const auto &v : o[QLatin1String("children")].toArray())
        node->children.push_back(nodeFromJson(v.toObject(), peByKey, node.get()));
    return node;
}

} // namespace

bool saveSession(const AnalysisResult &result, const QString &filePath, QString *error)
{
    QJsonObject root;
    root[QLatin1String("format")] = QStringLiteral("qdepends-session");
    root[QLatin1String("version")] = 1;
    root[QLatin1String("rootPath")] = result.rootPath;

    QJsonObject modules;
    for (const auto &rec : result.moduleList) {
        if (rec.pe)
            modules[rec.key] = peToJson(*rec.pe);
    }
    root[QLatin1String("modules")] = modules;

    QJsonArray records;
    for (const auto &rec : result.moduleList) {
        QJsonObject o;
        o[QLatin1String("key")] = rec.key;
        o[QLatin1String("path")] = rec.path;
        o[QLatin1String("status")] = int(rec.status);
        o[QLatin1String("delayLoadOnly")] = rec.delayLoadOnly;
        o[QLatin1String("cpuMismatch")] = rec.cpuMismatch;
        o[QLatin1String("hasMissingImports")] = rec.hasMissingImports;
        records.append(o);
    }
    root[QLatin1String("moduleList")] = records;

    if (result.root)
        root[QLatin1String("tree")] = nodeToJson(*result.root);

    QJsonArray log;
    for (const auto &entry : result.log) {
        QJsonObject o;
        o[QLatin1String("level")] = entry.level;
        o[QLatin1String("text")] = entry.text;
        log.append(o);
    }
    root[QLatin1String("log")] = log;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

AnalysisResultPtr loadSession(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return nullptr;
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (error)
            *error = parseError.errorString();
        return nullptr;
    }
    const QJsonObject root = doc.object();
    if (root[QLatin1String("format")].toString() != QLatin1String("qdepends-session")) {
        if (error)
            *error = QStringLiteral("Not a QDepends session file.");
        return nullptr;
    }

    auto result = std::make_shared<AnalysisResult>();
    result->rootPath = root[QLatin1String("rootPath")].toString();

    QHash<QString, core::PeInfoPtr> peByKey;
    const QJsonObject modules = root[QLatin1String("modules")].toObject();
    for (auto it = modules.begin(); it != modules.end(); ++it)
        peByKey.insert(it.key(), peFromJson(it.value().toObject()));

    for (const auto &v : root[QLatin1String("moduleList")].toArray()) {
        const QJsonObject o = v.toObject();
        ModuleRecord rec;
        rec.key = o[QLatin1String("key")].toString();
        rec.path = o[QLatin1String("path")].toString();
        rec.status = ModuleStatus(o[QLatin1String("status")].toInt());
        rec.delayLoadOnly = o[QLatin1String("delayLoadOnly")].toBool();
        rec.cpuMismatch = o[QLatin1String("cpuMismatch")].toBool();
        rec.hasMissingImports = o[QLatin1String("hasMissingImports")].toBool();
        rec.pe = peByKey.value(rec.key);
        result->moduleList.push_back(std::move(rec));
    }

    if (root.contains(QLatin1String("tree")))
        result->root = nodeFromJson(root[QLatin1String("tree")].toObject(), peByKey, nullptr);

    for (const auto &v : root[QLatin1String("log")].toArray()) {
        const QJsonObject o = v.toObject();
        LogEntry entry;
        entry.level = o[QLatin1String("level")].toInt();
        entry.text = o[QLatin1String("text")].toString();
        result->log.append(entry);
        if (entry.level == LogEntry::Error)
            ++result->errorCount;
        else if (entry.level == LogEntry::Warning)
            ++result->warningCount;
    }
    result->moduleCount = int(result->moduleList.size());
    return result;
}

} // namespace session
