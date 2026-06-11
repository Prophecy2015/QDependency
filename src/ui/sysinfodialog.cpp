#include "ui/sysinfodialog.h"

#include "core/apiset.h"
#include "core/knowndlls.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSysInfo>
#include <QVBoxLayout>

#include <windows.h>

namespace ui {

QString SysInfoDialog::collectSystemInfo()
{
    QStringList lines;
    const auto add = [&lines](const QString &k, const QString &v) {
        lines.append(QStringLiteral("%1: %2").arg(k, -22).arg(v));
    };

    add(QStringLiteral("Computer Name"), QSysInfo::machineHostName());
    add(QStringLiteral("OS"), QSysInfo::prettyProductName());
    add(QStringLiteral("Kernel Version"), QSysInfo::kernelVersion());
    add(QStringLiteral("CPU Architecture"), QSysInfo::currentCpuArchitecture());

    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    add(QStringLiteral("Processors"), QString::number(si.dwNumberOfProcessors));
    add(QStringLiteral("Page Size"), QString::number(si.dwPageSize));

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        add(QStringLiteral("Physical Memory"),
            QStringLiteral("%1 MB total, %2 MB free")
                .arg(ms.ullTotalPhys / (1024 * 1024))
                .arg(ms.ullAvailPhys / (1024 * 1024)));
        add(QStringLiteral("Memory Load"), QStringLiteral("%1%").arg(ms.dwMemoryLoad));
    }

    wchar_t buf[MAX_PATH];
    if (GetWindowsDirectoryW(buf, MAX_PATH))
        add(QStringLiteral("Windows Directory"), QString::fromWCharArray(buf));
    if (GetSystemDirectoryW(buf, MAX_PATH))
        add(QStringLiteral("System Directory"), QString::fromWCharArray(buf));
    add(QStringLiteral("Current Directory"), QDir::currentPath());

    add(QStringLiteral("Local Time"),
        QDateTime::currentDateTime().toString(Qt::ISODate));
    add(QStringLiteral("API Set Contracts"),
        QString::number(core::ApiSetResolver::instance().entryCount()));
    add(QStringLiteral("Known DLLs"), QString::number(core::knownDlls().size()));

    lines.append(QString());
    lines.append(QStringLiteral("PATH:"));
    const QString path = qEnvironmentVariable("PATH");
    for (const QString &dir : path.split(QLatin1Char(';'), Qt::SkipEmptyParts))
        lines.append(QStringLiteral("  %1").arg(dir));

    return lines.join(QLatin1Char('\n'));
}

SysInfoDialog::SysInfoDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("System Information"));
    resize(640, 480);

    auto *layout = new QVBoxLayout(this);
    auto *edit = new QPlainTextEdit(this);
    edit->setReadOnly(true);
    QFont mono(QStringLiteral("Consolas"));
    mono.setPointSizeF(9.0);
    edit->setFont(mono);
    edit->setPlainText(collectSystemInfo());
    layout->addWidget(edit);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *copyBtn = new QPushButton(tr("Copy"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    buttons->addWidget(copyBtn);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(copyBtn, &QPushButton::clicked, this, [edit]() {
        QApplication::clipboard()->setText(edit->toPlainText());
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace ui
