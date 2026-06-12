#pragma once

#include "session/analysissession.h"
#include "session/exportmodel.h"
#include "session/importmodel.h"
#include "session/modulelistmodel.h"

#include <QMainWindow>

class QSortFilterProxyModel;
class QSplitter;
class QTableView;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

namespace ui {

class FindBar;
class LogWidget;
class PaneTitleBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    // setup
    void buildPanes();
    void buildMenusAndToolbar();
    void buildStatusBar();
    void restoreSettings();
    void saveSettings();

    // analysis
    void onAnalysisFinished(session::AnalysisResultPtr result);
    void populateTree();
    QTreeWidgetItem *createTreeItem(const session::ModuleNode &node, bool isRoot);
    QString treeText(const session::ModuleNode &node) const;
    void refreshTreeTexts();
    void setUiBusy(bool busy);

    // interaction
    void onTreeSelectionChanged();
    void onModuleListActivated(const QModelIndex &index);
    void updateFunctionPaneTitles(const session::ModuleNode *node);
    void selectModuleInTree(const QString &key);
    void copySelection();
    void showTreeContextMenu(const QPoint &pos);
    void showTableContextMenu(QTableView *view, const QPoint &pos);
    void showDisassembly(int exportSourceRow);   // export pane source row

    // find
    void onSearchChanged(const QString &text);
    void findInTree(bool forward);

    // file actions
    void saveSessionFile();
    void saveReport();
    void exportCsv();
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    void launchExternalViewer(const QString &path);
    void configureExternalViewer();

    void updateStatusCounts();
    void applyViewerFonts();

    // widgets
    QTreeWidget *m_tree = nullptr;
    QTableView *m_importView = nullptr;
    QTableView *m_exportView = nullptr;
    QTableView *m_moduleView = nullptr;
    LogWidget *m_log = nullptr;
    FindBar *m_findBar = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_topSplitter = nullptr;
    QSplitter *m_funcSplitter = nullptr;
    QWidget *m_treePane = nullptr;
    QWidget *m_importPane = nullptr;
    QWidget *m_exportPane = nullptr;
    QWidget *m_modulePane = nullptr;
    QWidget *m_logPane = nullptr;
    PaneTitleBar *m_importTitle = nullptr;
    PaneTitleBar *m_exportTitle = nullptr;
    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;

    // models
    session::ImportModel *m_importModel = nullptr;
    session::ExportModel *m_exportModel = nullptr;
    session::ModuleListModel *m_moduleModel = nullptr;
    QSortFilterProxyModel *m_importProxy = nullptr;
    QSortFilterProxyModel *m_exportProxy = nullptr;
    QSortFilterProxyModel *m_moduleProxy = nullptr;

    // actions
    QAction *m_actFullPaths = nullptr;
    QAction *m_actUndecorate = nullptr;
    QAction *m_actRefresh = nullptr;
    QAction *m_actSaveSession = nullptr;
    QAction *m_actSaveReport = nullptr;
    QAction *m_actExportCsv = nullptr;
    QMenu *m_recentMenu = nullptr;

    // state
    session::AnalysisSession m_session;
    session::AnalysisResultPtr m_result;
    QString m_currentFile;
    QStringList m_recentFiles;
    QString m_externalViewerCmd;
    QList<QTreeWidgetItem *> m_searchMatches;
    int m_searchPos = -1;
};

} // namespace ui
