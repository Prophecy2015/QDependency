#include "ui/mainwindow.h"

#include "session/sessionserializer.h"
#include "ui/findbar.h"
#include "ui/iconfactory.h"
#include "ui/logwidget.h"
#include "ui/panetitlebar.h"
#include "ui/sysinfodialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <functional>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace ui {

namespace {

constexpr int kNodeRole = Qt::UserRole;
constexpr int kMaxRecent = 8;

const session::ModuleNode *nodeOf(const QTreeWidgetItem *item)
{
    if (!item)
        return nullptr;
    return static_cast<const session::ModuleNode *>(
        item->data(0, kNodeRole).value<void *>());
}

QWidget *wrapPane(PaneTitleBar *title, QWidget *content)
{
    auto *pane = new QWidget;
    auto *layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(title);
    layout->addWidget(content);
    return pane;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("QDepends"));
    setWindowIcon(IconFactory::appIcon());
    setAcceptDrops(true);
    resize(1280, 820);

    buildPanes();
    buildMenusAndToolbar();
    buildStatusBar();
    restoreSettings();
    applyViewerFonts();
    setUiBusy(false);

    connect(&m_session, &session::AnalysisSession::progressText, this,
            [this](const QString &name) {
                m_statusLeft->setText(tr("Analyzing %1...").arg(name));
            });
    connect(&m_session, &session::AnalysisSession::finished, this,
            &MainWindow::onAnalysisFinished, Qt::QueuedConnection);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------- setup

void MainWindow::buildPanes()
{
    // dependency tree
    m_tree = new QTreeWidget;
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treePane = wrapPane(new PaneTitleBar(tr("Dependency Tree")), m_tree);

    const auto makeTable = [this](QAbstractTableModel *model,
                                  QSortFilterProxyModel *&proxy, int sortRole) {
        auto *view = new QTableView;
        proxy = new QSortFilterProxyModel(this);
        proxy->setSourceModel(model);
        proxy->setSortRole(sortRole);
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxy->setFilterKeyColumn(-1);
        view->setModel(proxy);
        view->setSortingEnabled(true);
        view->horizontalHeader()->setSortIndicatorShown(true);
        view->horizontalHeader()->setHighlightSections(false);
        view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        view->verticalHeader()->hide();
        view->verticalHeader()->setDefaultSectionSize(20);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setShowGrid(false);
        view->setWordWrap(false);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QTableView::customContextMenuRequested, this,
                [this, view](const QPoint &pos) { showTableContextMenu(view, pos); });
        return view;
    };

    m_importModel = new session::ImportModel(this);
    m_importView = makeTable(m_importModel, m_importProxy, session::ImportModel::SortRole);
    m_importTitle = new PaneTitleBar(tr("Parent Imports"));
    m_importPane = wrapPane(m_importTitle, m_importView);

    m_exportModel = new session::ExportModel(this);
    m_exportView = makeTable(m_exportModel, m_exportProxy, session::ExportModel::SortRole);
    m_exportTitle = new PaneTitleBar(tr("Exports"));
    m_exportPane = wrapPane(m_exportTitle, m_exportView);

    m_moduleModel = new session::ModuleListModel(this);
    m_moduleView = makeTable(m_moduleModel, m_moduleProxy,
                             session::ModuleListModel::SortRole);
    m_modulePane = wrapPane(new PaneTitleBar(tr("Module List")), m_moduleView);

    m_log = new LogWidget;
    m_logPane = wrapPane(new PaneTitleBar(tr("Log")), m_log);

    m_funcSplitter = new QSplitter(Qt::Vertical);
    m_funcSplitter->addWidget(m_importPane);
    m_funcSplitter->addWidget(m_exportPane);

    m_topSplitter = new QSplitter(Qt::Horizontal);
    m_topSplitter->addWidget(m_treePane);
    m_topSplitter->addWidget(m_funcSplitter);
    m_topSplitter->setStretchFactor(0, 2);
    m_topSplitter->setStretchFactor(1, 3);

    m_mainSplitter = new QSplitter(Qt::Vertical);
    m_mainSplitter->addWidget(m_topSplitter);
    m_mainSplitter->addWidget(m_modulePane);
    m_mainSplitter->addWidget(m_logPane);
    m_mainSplitter->setStretchFactor(0, 5);
    m_mainSplitter->setStretchFactor(1, 3);
    m_mainSplitter->setStretchFactor(2, 1);

    m_findBar = new FindBar;
    m_findBar->hide();

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_findBar);
    layout->addWidget(m_mainSplitter);
    setCentralWidget(central);

    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            &MainWindow::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            &MainWindow::showTreeContextMenu);
    connect(m_moduleView, &QTableView::doubleClicked, this,
            &MainWindow::onModuleListActivated);

    connect(m_findBar, &FindBar::searchChanged, this, &MainWindow::onSearchChanged);
    connect(m_findBar, &FindBar::findNext, this, [this]() { findInTree(true); });
    connect(m_findBar, &FindBar::findPrevious, this, [this]() { findInTree(false); });
    connect(m_findBar, &FindBar::closed, this, [this]() { onSearchChanged(QString()); });
}

void MainWindow::buildMenusAndToolbar()
{
    const QColor accent(0xcc, 0xcc, 0xcc);

    // ---- File
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *actOpen = fileMenu->addAction(IconFactory::glyphIcon(QChar(0xE838), accent),
                                           tr("&Open..."), QKeySequence::Open, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open PE File"), QString(),
            tr("PE Files (*.exe *.dll *.sys *.ocx *.cpl *.drv *.scr *.mui *.ax *.efi);;"
               "QDepends Session (*.qds);;All Files (*)"));
        if (!path.isEmpty())
            openFile(path);
    });
    Q_UNUSED(actOpen)

    m_actSaveSession = fileMenu->addAction(
        IconFactory::glyphIcon(QChar(0xE74E), accent), tr("&Save Session As..."),
        QKeySequence::Save, this, &MainWindow::saveSessionFile);
    m_actSaveReport = fileMenu->addAction(tr("Save Report As &Text..."), this,
                                          &MainWindow::saveReport);
    m_actExportCsv = fileMenu->addAction(tr("Export Module List As &CSV..."), this,
                                         &MainWindow::exportCsv);
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(tr("&Recent Files"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    // ---- Edit
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(IconFactory::glyphIcon(QChar(0xE8C8), accent), tr("&Copy"),
                        QKeySequence::Copy, this, &MainWindow::copySelection);
    editMenu->addSeparator();
    editMenu->addAction(IconFactory::glyphIcon(QChar(0xE721), accent), tr("&Find..."),
                        QKeySequence::Find, this, [this]() {
        m_findBar->show();
        m_findBar->focusInput();
    });
    QAction *actFindNext = editMenu->addAction(tr("Find &Next"), QKeySequence(Qt::Key_F3),
                                               this, [this]() { findInTree(true); });
    QAction *actFindPrev = editMenu->addAction(tr("Find &Previous"),
                                               QKeySequence(Qt::SHIFT | Qt::Key_F3), this,
                                               [this]() { findInTree(false); });
    Q_UNUSED(actFindNext)
    Q_UNUSED(actFindPrev)
    editMenu->addSeparator();
    editMenu->addAction(tr("Clear &Log"), this, [this]() { m_log->clear(); });

    // ---- View
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_actFullPaths = viewMenu->addAction(IconFactory::glyphIcon(QChar(0xE71B), accent),
                                         tr("Full &Paths"));
    m_actFullPaths->setCheckable(true);
    m_actFullPaths->setShortcut(QKeySequence(Qt::Key_F9));
    connect(m_actFullPaths, &QAction::toggled, this, [this](bool on) {
        m_moduleModel->setFullPaths(on);
        refreshTreeTexts();
    });

    m_actUndecorate = viewMenu->addAction(IconFactory::glyphIcon(QChar(0xE943), accent),
                                          tr("&Undecorate C++ Functions"));
    m_actUndecorate->setCheckable(true);
    m_actUndecorate->setShortcut(QKeySequence(Qt::Key_F10));
    connect(m_actUndecorate, &QAction::toggled, this, [this](bool on) {
        m_importModel->setUndecorate(on);
        m_exportModel->setUndecorate(on);
    });

    viewMenu->addSeparator();
    viewMenu->addAction(IconFactory::glyphIcon(QChar(0xE70D), accent), tr("&Expand All"),
                        QKeySequence(Qt::CTRL | Qt::Key_E), this,
                        [this]() { m_tree->expandAll(); });
    viewMenu->addAction(IconFactory::glyphIcon(QChar(0xE70E), accent), tr("Co&llapse All"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this,
                        [this]() { m_tree->collapseAll(); });
    m_actRefresh = viewMenu->addAction(IconFactory::glyphIcon(QChar(0xE72C), accent),
                                       tr("&Refresh"), QKeySequence(Qt::Key_F5), this,
                                       [this]() {
        if (!m_currentFile.isEmpty() && !m_currentFile.endsWith(QLatin1String(".qds")))
            openFile(m_currentFile);
    });
    viewMenu->addSeparator();

    const auto addPaneToggle = [viewMenu](const QString &title, QWidget *pane) {
        QAction *act = viewMenu->addAction(title);
        act->setCheckable(true);
        act->setChecked(true);
        connect(act, &QAction::toggled, pane, &QWidget::setVisible);
        return act;
    };
    addPaneToggle(tr("Show Parent &Imports Pane"), m_importPane);
    addPaneToggle(tr("Show Export&s Pane"), m_exportPane);
    addPaneToggle(tr("Show &Module List Pane"), m_modulePane);
    addPaneToggle(tr("Show Lo&g Pane"), m_logPane);
    viewMenu->addSeparator();
    viewMenu->addAction(IconFactory::glyphIcon(QChar(0xE946), accent),
                        tr("System &Information..."), this, [this]() {
        SysInfoDialog dialog(this);
        dialog.exec();
    });

    // ---- Options
    QMenu *optionsMenu = menuBar()->addMenu(tr("&Options"));
    optionsMenu->addAction(tr("Configure &External Viewer..."), this,
                           &MainWindow::configureExternalViewer);

    // ---- Help
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About QDepends"), this, [this]() {
        QMessageBox::about(
            this, tr("About QDepends"),
            tr("<b>QDepends 1.0</b><br/>A static PE dependency analyzer, "
               "functionally aligned with Dependency Walker.<br/><br/>"
               "Supports 32-bit (PE32) and 64-bit (PE32+) modules, API set "
               "redirection, KnownDLLs, side-by-side manifests and forwarded "
               "exports.<br/><br/>Built with Qt %1.")
                .arg(QLatin1String(qVersion())));
    });

    // ---- toolbar
    QToolBar *toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->addAction(actOpen);
    toolbar->addAction(m_actSaveSession);
    toolbar->addSeparator();
    toolbar->addAction(m_actFullPaths);
    toolbar->addAction(m_actUndecorate);
    toolbar->addSeparator();
    toolbar->addAction(m_actRefresh);
}

void MainWindow::buildStatusBar()
{
    m_statusLeft = new QLabel(tr("Ready"));
    m_statusRight = new QLabel;
    statusBar()->addWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusRight);
    statusBar()->setSizeGripEnabled(true);
}

void MainWindow::applyViewerFonts()
{
    QFont mono(QStringLiteral("Consolas"));
    mono.setPointSizeF(9.0);
    m_tree->setFont(mono);
    m_importView->setFont(mono);
    m_exportView->setFont(mono);
    m_moduleView->setFont(mono);
}

void MainWindow::restoreSettings()
{
    QSettings s;
    restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
    if (s.contains(QStringLiteral("mainSplitter")))
        m_mainSplitter->restoreState(s.value(QStringLiteral("mainSplitter")).toByteArray());
    if (s.contains(QStringLiteral("topSplitter")))
        m_topSplitter->restoreState(s.value(QStringLiteral("topSplitter")).toByteArray());
    m_recentFiles = s.value(QStringLiteral("recentFiles")).toStringList();
    m_externalViewerCmd = s.value(QStringLiteral("externalViewer")).toString();
    m_actFullPaths->setChecked(s.value(QStringLiteral("fullPaths"), false).toBool());
    m_actUndecorate->setChecked(s.value(QStringLiteral("undecorate"), false).toBool());
    rebuildRecentMenu();
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("geometry"), saveGeometry());
    s.setValue(QStringLiteral("mainSplitter"), m_mainSplitter->saveState());
    s.setValue(QStringLiteral("topSplitter"), m_topSplitter->saveState());
    s.setValue(QStringLiteral("recentFiles"), m_recentFiles);
    s.setValue(QStringLiteral("externalViewer"), m_externalViewerCmd);
    s.setValue(QStringLiteral("fullPaths"), m_actFullPaths->isChecked());
    s.setValue(QStringLiteral("undecorate"), m_actUndecorate->isChecked());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            openFile(url.toLocalFile());
            return;
        }
    }
}

// ---------------------------------------------------------------- analysis

void MainWindow::openFile(const QString &path)
{
    if (m_session.isRunning())
        return;

    const QFileInfo fi(path);
    if (!fi.exists()) {
        QMessageBox::warning(this, tr("QDepends"), tr("File not found: %1").arg(path));
        return;
    }

    m_currentFile = QDir::toNativeSeparators(fi.absoluteFilePath());
    addRecentFile(m_currentFile);

    if (path.endsWith(QLatin1String(".qds"), Qt::CaseInsensitive)) {
        QString error;
        auto result = session::loadSession(path, &error);
        if (!result) {
            QMessageBox::warning(this, tr("QDepends"),
                                 tr("Cannot load session: %1").arg(error));
            return;
        }
        onAnalysisFinished(result);
        return;
    }

    setUiBusy(true);
    m_statusLeft->setText(tr("Analyzing %1...").arg(fi.fileName()));
    m_session.start(fi.absoluteFilePath());
}

void MainWindow::setUiBusy(bool busy)
{
    const bool hasResult = m_result != nullptr;
    m_actSaveSession->setEnabled(!busy && hasResult);
    m_actSaveReport->setEnabled(!busy && hasResult);
    m_actExportCsv->setEnabled(!busy && hasResult);
    m_actRefresh->setEnabled(!busy && hasResult);
}

void MainWindow::onAnalysisFinished(session::AnalysisResultPtr result)
{
    m_result = result;
    setWindowTitle(QStringLiteral("QDepends — %1")
                       .arg(QDir::toNativeSeparators(result->rootPath)));

    populateTree();
    m_moduleModel->setResult(result);
    m_moduleView->resizeColumnsToContents();
    m_moduleView->horizontalHeader()->setSectionResizeMode(
        session::ModuleListModel::ColModule, QHeaderView::Interactive);
    m_log->setEntries(result->log);
    m_log->scrollToBottom();

    setUiBusy(false);
    m_statusLeft->setText(tr("Ready"));
    updateStatusCounts();
}

void MainWindow::updateStatusCounts()
{
    if (!m_result) {
        m_statusRight->clear();
        return;
    }
    m_statusRight->setText(tr("Modules: %1   Errors: %2   Warnings: %3")
                               .arg(m_result->moduleCount)
                               .arg(m_result->errorCount)
                               .arg(m_result->warningCount));
}

QString MainWindow::treeText(const session::ModuleNode &node) const
{
    QString name;
    if (m_actFullPaths->isChecked() && !node.resolvedPath.isEmpty())
        name = QDir::toNativeSeparators(node.resolvedPath);
    else
        name = node.rawName.toUpper();
    if (!node.apiSetHost.isEmpty())
        name += QStringLiteral("  →  %1").arg(node.apiSetHost.toUpper());
    return name;
}

QTreeWidgetItem *MainWindow::createTreeItem(const session::ModuleNode &node, bool isRoot)
{
    auto *item = new QTreeWidgetItem(QStringList{treeText(node)});
    item->setData(0, kNodeRole,
                  QVariant::fromValue<void *>(const_cast<session::ModuleNode *>(&node)));
    item->setIcon(0, IconFactory::moduleIcon(node, isRoot));

    QString tip = node.resolvedPath.isEmpty() ? tr("Module not found")
                                              : QDir::toNativeSeparators(node.resolvedPath);
    if (!node.searchNote.isEmpty())
        tip += QStringLiteral("\n%1").arg(node.searchNote);
    if (node.delayLoad)
        tip += QStringLiteral("\n%1").arg(tr("Delay-load dependency"));
    if (node.forwarded)
        tip += QStringLiteral("\n%1").arg(tr("Forwarded dependency"));
    if (node.duplicate)
        tip += QStringLiteral("\n%1").arg(tr("Duplicate module (already expanded above)"));
    item->setToolTip(0, tip);

    if (node.status != session::ModuleStatus::Ok)
        item->setForeground(0, QColor(0xf4, 0x87, 0x71));
    else if (node.duplicate)
        item->setForeground(0, QColor(0x8a, 0x8a, 0x8a));
    else if (node.cpuMismatch || node.hasMissingImports)
        item->setForeground(0, QColor(0xcc, 0xa7, 0x00));

    for (const auto &child : node.children)
        item->addChild(createTreeItem(*child, false));
    return item;
}

void MainWindow::populateTree()
{
    m_tree->clear();
    m_searchMatches.clear();
    m_searchPos = -1;
    if (!m_result || !m_result->root)
        return;

    m_tree->setUpdatesEnabled(false);
    QTreeWidgetItem *rootItem = createTreeItem(*m_result->root, true);
    m_tree->addTopLevelItem(rootItem);
    m_tree->expandToDepth(1);
    m_tree->setUpdatesEnabled(true);
    m_tree->setCurrentItem(rootItem);
}

void MainWindow::refreshTreeTexts()
{
    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *item) {
        if (const auto *node = nodeOf(item))
            item->setText(0, treeText(*node));
        for (int i = 0; i < item->childCount(); ++i)
            walk(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        walk(m_tree->topLevelItem(i));
}

// ---------------------------------------------------------------- interaction

void MainWindow::onTreeSelectionChanged()
{
    const auto *node = nodeOf(m_tree->currentItem());
    m_importModel->setNode(node);
    m_exportModel->setNode(node);
    m_importView->resizeColumnsToContents();
    m_exportView->resizeColumnsToContents();
    updateFunctionPaneTitles(node);
    if (node) {
        m_statusLeft->setText(node->resolvedPath.isEmpty()
                                  ? node->rawName
                                  : QDir::toNativeSeparators(node->resolvedPath));
    }
}

void MainWindow::updateFunctionPaneTitles(const session::ModuleNode *node)
{
    if (!node) {
        m_importTitle->setTitle(tr("Parent Imports"));
        m_exportTitle->setTitle(tr("Exports"));
        return;
    }
    const QString moduleName = QFileInfo(node->rawName).fileName();
    if (node->parent) {
        m_importTitle->setTitle(tr("Parent Imports — %1 ← %2")
                                    .arg(node->parent->rawName.toUpper(),
                                         moduleName.toUpper()));
    } else {
        m_importTitle->setTitle(tr("Parent Imports — (root module)"));
    }
    m_exportTitle->setTitle(tr("Exports — %1").arg(moduleName.toUpper()));
}

void MainWindow::onModuleListActivated(const QModelIndex &index)
{
    const QModelIndex src = m_moduleProxy->mapToSource(index);
    if (const auto *rec = m_moduleModel->recordAt(src.row()))
        selectModuleInTree(rec->key);
}

void MainWindow::selectModuleInTree(const QString &key)
{
    QTreeWidgetItem *found = nullptr;
    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *item) {
        if (found)
            return;
        if (const auto *node = nodeOf(item); node && node->moduleKey() == key) {
            found = item;
            return;
        }
        for (int i = 0; i < item->childCount() && !found; ++i)
            walk(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount() && !found; ++i)
        walk(m_tree->topLevelItem(i));
    if (found) {
        m_tree->setCurrentItem(found);
        m_tree->scrollToItem(found, QAbstractItemView::PositionAtCenter);
        m_tree->setFocus();
    }
}

void MainWindow::copySelection()
{
    QWidget *focus = QApplication::focusWidget();
    if (focus == m_tree) {
        if (const auto *node = nodeOf(m_tree->currentItem())) {
            QApplication::clipboard()->setText(
                node->resolvedPath.isEmpty()
                    ? node->rawName
                    : QDir::toNativeSeparators(node->resolvedPath));
        }
        return;
    }
    if (auto *view = qobject_cast<QTableView *>(focus)) {
        const auto rows = view->selectionModel()->selectedRows();
        QStringList lines;
        const auto *model = view->model();
        for (const QModelIndex &rowIndex : rows) {
            QStringList cells;
            for (int c = 0; c < model->columnCount(); ++c) {
                const QString text =
                    model->index(rowIndex.row(), c).data(Qt::DisplayRole).toString();
                if (!text.isEmpty())
                    cells.append(text);
            }
            lines.append(cells.join(QLatin1Char('\t')));
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
        return;
    }
    if (focus == m_log) {
        QStringList lines;
        const auto items = m_log->selectedItems();
        for (const auto *it : items)
            lines.append(it->text());
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    }
}

void MainWindow::showTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    const auto *node = nodeOf(item);
    if (!node)
        return;

    QMenu menu(this);
    QAction *expandSub = menu.addAction(tr("Expand Subtree"));
    menu.addSeparator();
    QAction *copyPath = menu.addAction(tr("Copy File Path"));
    QAction *openExplorer = menu.addAction(tr("Show in Explorer"));
    QAction *extViewer = menu.addAction(tr("View Module in External Viewer"));
    menu.addSeparator();
    QAction *locate = menu.addAction(tr("Locate in Module List"));

    const bool hasPath = !node->resolvedPath.isEmpty();
    copyPath->setEnabled(hasPath);
    openExplorer->setEnabled(hasPath);
    extViewer->setEnabled(hasPath);

    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == expandSub) {
        std::function<void(QTreeWidgetItem *)> expand = [&](QTreeWidgetItem *it) {
            it->setExpanded(true);
            for (int i = 0; i < it->childCount(); ++i)
                expand(it->child(i));
        };
        expand(item);
    } else if (chosen == copyPath) {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(node->resolvedPath));
    } else if (chosen == openExplorer) {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                {QStringLiteral("/select,"),
                                 QDir::toNativeSeparators(node->resolvedPath)});
    } else if (chosen == extViewer) {
        launchExternalViewer(node->resolvedPath);
    } else if (chosen == locate) {
        const QString key = node->moduleKey();
        for (int row = 0; row < m_moduleProxy->rowCount(); ++row) {
            const QModelIndex idx = m_moduleProxy->index(row, 0);
            const auto *rec =
                m_moduleModel->recordAt(m_moduleProxy->mapToSource(idx).row());
            if (rec && rec->key == key) {
                m_moduleView->selectRow(row);
                m_moduleView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                m_moduleView->setFocus();
                break;
            }
        }
    }
}

void MainWindow::showTableContextMenu(QTableView *view, const QPoint &pos)
{
    QMenu menu(this);
    QAction *copyRows = menu.addAction(tr("Copy"));
    QAction *chosen = menu.exec(view->viewport()->mapToGlobal(pos));
    if (chosen == copyRows) {
        view->setFocus();
        copySelection();
    }
}

// ---------------------------------------------------------------- find

void MainWindow::onSearchChanged(const QString &text)
{
    m_importProxy->setFilterFixedString(text);
    m_exportProxy->setFilterFixedString(text);
    m_moduleProxy->setFilterFixedString(text);

    m_searchMatches.clear();
    m_searchPos = -1;
    if (!text.isEmpty()) {
        std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *item) {
            if (item->text(0).contains(text, Qt::CaseInsensitive))
                m_searchMatches.append(item);
            for (int i = 0; i < item->childCount(); ++i)
                walk(item->child(i));
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            walk(m_tree->topLevelItem(i));
    }
    m_findBar->setMatchInfo(0, int(m_searchMatches.size()));
}

void MainWindow::findInTree(bool forward)
{
    if (m_searchMatches.isEmpty()) {
        m_findBar->setMatchInfo(0, 0);
        return;
    }
    const int n = int(m_searchMatches.size());
    m_searchPos = forward ? (m_searchPos + 1) % n : (m_searchPos - 1 + n) % n;
    QTreeWidgetItem *item = m_searchMatches.at(m_searchPos);
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    m_findBar->setMatchInfo(m_searchPos + 1, n);
}

// ---------------------------------------------------------------- file actions

void MainWindow::saveSessionFile()
{
    if (!m_result)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Session"), QString(), tr("QDepends Session (*.qds)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!session::saveSession(*m_result, path, &error))
        QMessageBox::warning(this, tr("QDepends"),
                             tr("Cannot save session: %1").arg(error));
}

void MainWindow::saveReport()
{
    if (!m_result)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Report"), QString(), tr("Text Files (*.txt)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("QDepends"), file.errorString());
        return;
    }
    QTextStream out(&file);

    out << "*| QDepends report for " << QDir::toNativeSeparators(m_result->rootPath)
        << " |*\n\n";
    out << "*| Dependency Tree |*\n\n";
    std::function<void(const session::ModuleNode &, int)> walk =
        [&](const session::ModuleNode &node, int depth) {
            out << QString(depth * 4, QLatin1Char(' '));
            QString flags;
            if (node.delayLoad)
                flags += QLatin1String(" [delay-load]");
            if (node.forwarded)
                flags += QLatin1String(" [forwarded]");
            if (node.duplicate)
                flags += QLatin1String(" [duplicate]");
            if (node.status == session::ModuleStatus::Missing)
                flags += QLatin1String(" [MISSING]");
            else if (node.status == session::ModuleStatus::Invalid)
                flags += QLatin1String(" [INVALID]");
            out << (node.resolvedPath.isEmpty()
                        ? node.rawName
                        : QDir::toNativeSeparators(node.resolvedPath))
                << flags << "\n";
            for (const auto &c : node.children)
                walk(*c, depth + 1);
        };
    if (m_result->root)
        walk(*m_result->root, 0);

    out << "\n*| Module List |*\n\n";
    const int cols = m_moduleModel->columnCount();
    for (int row = 0; row < m_moduleModel->rowCount(); ++row) {
        QStringList cells;
        for (int c = 1; c < cols; ++c)
            cells.append(m_moduleModel->index(row, c).data().toString());
        out << cells.join(QLatin1String(" | ")) << "\n";
    }

    out << "\n*| Log |*\n\n";
    for (const auto &e : m_result->log)
        out << e.text << "\n";

    m_statusLeft->setText(tr("Report saved to %1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::exportCsv()
{
    if (!m_result)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QString(), tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("QDepends"), file.errorString());
        return;
    }
    QTextStream out(&file);
    const auto quote = [](QString s) {
        if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')))
            s = QLatin1Char('"') + s.replace(QLatin1Char('"'), QLatin1String("\"\""))
                + QLatin1Char('"');
        return s;
    };
    const int cols = m_moduleModel->columnCount();
    QStringList header;
    for (int c = 1; c < cols; ++c)
        header.append(quote(m_moduleModel->headerData(c, Qt::Horizontal,
                                                      Qt::DisplayRole).toString()));
    out << header.join(QLatin1Char(',')) << "\n";
    for (int row = 0; row < m_moduleModel->rowCount(); ++row) {
        QStringList cells;
        for (int c = 1; c < cols; ++c)
            cells.append(quote(m_moduleModel->index(row, c).data().toString()));
        out << cells.join(QLatin1Char(',')) << "\n";
    }
    m_statusLeft->setText(tr("CSV exported to %1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::addRecentFile(const QString &path)
{
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > kMaxRecent)
        m_recentFiles.removeLast();
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    for (const QString &path : std::as_const(m_recentFiles)) {
        m_recentMenu->addAction(path, this, [this, path]() { openFile(path); });
    }
    m_recentMenu->setEnabled(!m_recentFiles.isEmpty());
}

void MainWindow::configureExternalViewer()
{
    bool ok = false;
    const QString cmd = QInputDialog::getText(
        this, tr("External Viewer"),
        tr("Command line (%1 is replaced with the module path):"), QLineEdit::Normal,
        m_externalViewerCmd.isEmpty() ? QStringLiteral("notepad.exe %1")
                                      : m_externalViewerCmd,
        &ok);
    if (ok)
        m_externalViewerCmd = cmd;
}

void MainWindow::launchExternalViewer(const QString &path)
{
    if (m_externalViewerCmd.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        return;
    }
    QString cmd = m_externalViewerCmd;
    cmd.replace(QLatin1String("%1"),
                QLatin1Char('"') + QDir::toNativeSeparators(path) + QLatin1Char('"'));
    QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), cmd});
}

} // namespace ui
