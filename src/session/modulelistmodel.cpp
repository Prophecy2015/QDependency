#include "session/modulelistmodel.h"

#include "core/peparser.h"
#include "ui/iconfactory.h"

#include <QColor>
#include <QFileInfo>
#include <QLocale>

namespace session {

namespace {

QString hex32(uint32_t v)
{
    return QStringLiteral("0x%1").arg(v, 8, 16, QLatin1Char('0')).toUpper()
        .replace(QLatin1String("0X"), QLatin1String("0x"));
}

QString hex64(uint64_t v)
{
    const int width = v > 0xffffffffull ? 16 : 8;
    return QStringLiteral("0x%1").arg(v, width, 16, QLatin1Char('0')).toUpper()
        .replace(QLatin1String("0X"), QLatin1String("0x"));
}

QString timeStampToString(uint32_t t)
{
    if (t == 0)
        return QStringLiteral("N/A");
    return QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::utc())
        .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace

ModuleListModel::ModuleListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ModuleListModel::setResult(const AnalysisResultPtr &result)
{
    beginResetModel();
    m_result = result;
    endResetModel();
}

void ModuleListModel::setFullPaths(bool on)
{
    if (m_fullPaths == on)
        return;
    beginResetModel();
    m_fullPaths = on;
    endResetModel();
}

int ModuleListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_result ? 0 : int(m_result->moduleList.size());
}

int ModuleListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

const ModuleRecord *ModuleListModel::recordAt(int row) const
{
    if (!m_result || row < 0 || row >= int(m_result->moduleList.size()))
        return nullptr;
    return &m_result->moduleList[row];
}

QVariant ModuleListModel::data(const QModelIndex &index, int role) const
{
    const ModuleRecord *rec = recordAt(index.row());
    if (!rec)
        return {};
    const auto pe = rec->pe;
    const bool ok = pe && pe->valid;
    const QString na = QStringLiteral("N/A");

    if (role == Qt::DisplayRole || role == SortRole) {
        switch (index.column()) {
        case ColIcon:
            return role == SortRole ? QVariant(int(rec->status)) : QVariant();
        case ColModule: {
            const QString name = m_fullPaths
                                     ? rec->path
                                     : QFileInfo(rec->path).fileName().toUpper();
            return name;
        }
        case ColFileTimeStamp:
            if (!ok || !pe->fileTime.isValid())
                return role == SortRole ? QVariant(qint64(0)) : QVariant(na);
            return role == SortRole
                       ? QVariant(pe->fileTime.toSecsSinceEpoch())
                       : QVariant(pe->fileTime.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        case ColLinkTimeStamp:
            if (!ok)
                return role == SortRole ? QVariant(qint64(0)) : QVariant(na);
            return role == SortRole ? QVariant(qint64(pe->linkTimeStamp))
                                    : QVariant(timeStampToString(pe->linkTimeStamp));
        case ColFileSize:
            if (!ok)
                return role == SortRole ? QVariant(qint64(0)) : QVariant(na);
            return role == SortRole ? QVariant(pe->fileSize)
                                    : QVariant(QLocale::c().toString(pe->fileSize));
        case ColAttributes:     return ok ? QVariant(pe->fileAttributes) : QVariant(na);
        case ColLinkChecksum:
            return ok ? QVariant(hex32(pe->linkChecksum)) : QVariant(na);
        case ColRealChecksum:
            return ok ? QVariant(hex32(pe->realChecksum)) : QVariant(na);
        case ColCpu:
            return ok ? QVariant(core::cpuTypeName(pe->cpu)) : QVariant(na);
        case ColSubsystem:
            return ok ? QVariant(core::subsystemName(pe->subsystem)) : QVariant(na);
        case ColSymbols:        return ok ? QVariant(pe->symbols) : QVariant(na);
        case ColPreferredBase:
            if (!ok)
                return role == SortRole ? QVariant(qint64(0)) : QVariant(na);
            return role == SortRole ? QVariant(qint64(pe->preferredBase))
                                    : QVariant(hex64(pe->preferredBase));
        case ColActualBase:     return tr("N/A");
        case ColVirtualSize:
            if (!ok)
                return role == SortRole ? QVariant(qint64(0)) : QVariant(na);
            return role == SortRole ? QVariant(qint64(pe->sizeOfImage))
                                    : QVariant(hex32(pe->sizeOfImage));
        case ColLoadOrder:      return tr("N/A");
        case ColFileVer:        return ok && !pe->fileVer.isEmpty() ? QVariant(pe->fileVer) : QVariant(na);
        case ColProductVer:     return ok && !pe->productVer.isEmpty() ? QVariant(pe->productVer) : QVariant(na);
        case ColImageVer:       return ok ? QVariant(pe->imageVer) : QVariant(na);
        case ColLinkerVer:      return ok ? QVariant(pe->linkerVer) : QVariant(na);
        case ColOsVer:          return ok ? QVariant(pe->osVer) : QVariant(na);
        case ColSubsystemVer:   return ok ? QVariant(pe->subsystemVer) : QVariant(na);
        }
        return {};
    }

    if (role == Qt::DecorationRole && index.column() == ColIcon)
        return ui::IconFactory::moduleIcon(*rec);

    if (role == Qt::ForegroundRole) {
        if (rec->status == ModuleStatus::Missing && rec->delayLoadOnly)
            return QColor(0xcc, 0xa7, 0x00);
        if (rec->status != ModuleStatus::Ok)
            return QColor(0xf4, 0x87, 0x71);
        if (rec->cpuMismatch || rec->hasMissingImports)
            return QColor(0xcc, 0xa7, 0x00);
        return {};
    }

    return {};
}

QVariant ModuleListModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColIcon:          return {};
    case ColModule:        return tr("Module");
    case ColFileTimeStamp: return tr("File Time Stamp");
    case ColLinkTimeStamp: return tr("Link Time Stamp");
    case ColFileSize:      return tr("File Size");
    case ColAttributes:    return tr("Attr.");
    case ColLinkChecksum:  return tr("Link Checksum");
    case ColRealChecksum:  return tr("Real Checksum");
    case ColCpu:           return tr("CPU");
    case ColSubsystem:     return tr("Subsystem");
    case ColSymbols:       return tr("Symbols");
    case ColPreferredBase: return tr("Preferred Base");
    case ColActualBase:    return tr("Actual Base");
    case ColVirtualSize:   return tr("Virtual Size");
    case ColLoadOrder:     return tr("Load Order");
    case ColFileVer:       return tr("File Ver");
    case ColProductVer:    return tr("Product Ver");
    case ColImageVer:      return tr("Image Ver");
    case ColLinkerVer:     return tr("Linker Ver");
    case ColOsVer:         return tr("OS Ver");
    case ColSubsystemVer:  return tr("Subsystem Ver");
    }
    return {};
}

} // namespace session
