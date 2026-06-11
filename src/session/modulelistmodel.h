#pragma once

#include "session/modulenode.h"

#include <QAbstractTableModel>

namespace session {

// "Module List" pane: every unique module, with the depends.exe column set.
class ModuleListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColIcon = 0,
        ColModule,
        ColFileTimeStamp,
        ColLinkTimeStamp,
        ColFileSize,
        ColAttributes,
        ColLinkChecksum,
        ColRealChecksum,
        ColCpu,
        ColSubsystem,
        ColSymbols,
        ColPreferredBase,
        ColActualBase,
        ColVirtualSize,
        ColLoadOrder,
        ColFileVer,
        ColProductVer,
        ColImageVer,
        ColLinkerVer,
        ColOsVer,
        ColSubsystemVer,
        ColCount
    };
    enum { SortRole = Qt::UserRole + 1 };

    explicit ModuleListModel(QObject *parent = nullptr);

    void setResult(const AnalysisResultPtr &result);
    void setFullPaths(bool on);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    const ModuleRecord *recordAt(int row) const;

private:
    AnalysisResultPtr m_result;
    bool m_fullPaths = true;
};

} // namespace session
