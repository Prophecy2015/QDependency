#include "ui/headerinfodialog.h"

#include "core/peparser.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <windows.h>

namespace ui {

namespace {

// ---- VS Code Dark+ palette used in the rich-text layout ----
const QString kColLabel   = QStringLiteral("#858585");
const QString kColValue   = QStringLiteral("#d4d4d4");
const QString kColNumber  = QStringLiteral("#9cdcfe");   // hex / numeric values
const QString kColSection = QStringLiteral("#569cd6");   // section headings
const QString kColOk      = QStringLiteral("#89d185");
const QString kColWarn    = QStringLiteral("#f48771");
const QString kColCardBg  = QStringLiteral("#252526");
const QString kColChipBg  = QStringLiteral("#37373d");
const QString kColChipTx  = QStringLiteral("#cccccc");

QString esc(const QString &s)
{
    return s.toHtmlEscaped();
}

QString hex(uint64_t v, int width)
{
    return QStringLiteral("0x%1").arg(v, width, 16, QLatin1Char('0')).toUpper()
        .replace(QLatin1String("0X"), QLatin1String("0x"));
}

QString machineName(uint16_t machine)
{
    switch (machine) {
    case IMAGE_FILE_MACHINE_I386:  return QStringLiteral("Intel 386 (x86)");
    case IMAGE_FILE_MACHINE_AMD64: return QStringLiteral("AMD64 (x64)");
    case 0xAA64:                   return QStringLiteral("ARM64");
    case IMAGE_FILE_MACHINE_ARM:   return QStringLiteral("ARM");
    case 0x01C4:                   return QStringLiteral("ARMv7 (Thumb-2)");
    case IMAGE_FILE_MACHINE_IA64:  return QStringLiteral("Intel Itanium");
    default:                       return QStringLiteral("Unknown");
    }
}

QStringList characteristicFlags(uint16_t c)
{
    struct Flag { uint16_t bit; const char *name; };
    static const Flag flags[] = {
        {0x0001, "RELOCS_STRIPPED"},   {0x0002, "EXECUTABLE_IMAGE"},
        {0x0004, "LINE_NUMS_STRIPPED"},{0x0008, "LOCAL_SYMS_STRIPPED"},
        {0x0010, "AGGRESSIVE_WS_TRIM"},{0x0020, "LARGE_ADDRESS_AWARE"},
        {0x0100, "32BIT_MACHINE"},     {0x0200, "DEBUG_STRIPPED"},
        {0x0400, "REMOVABLE_RUN_FROM_SWAP"}, {0x0800, "NET_RUN_FROM_SWAP"},
        {0x1000, "SYSTEM"},            {0x2000, "DLL"},
        {0x4000, "UP_SYSTEM_ONLY"},
    };
    QStringList out;
    for (const auto &f : flags)
        if (c & f.bit)
            out << QLatin1String(f.name);
    return out;
}

QStringList dllCharacteristicFlags(uint16_t c)
{
    struct Flag { uint16_t bit; const char *name; };
    static const Flag flags[] = {
        {0x0020, "HIGH_ENTROPY_VA"},      {0x0040, "DYNAMIC_BASE (ASLR)"},
        {0x0080, "FORCE_INTEGRITY"},      {0x0100, "NX_COMPAT (DEP)"},
        {0x0200, "NO_ISOLATION"},         {0x0400, "NO_SEH"},
        {0x0800, "NO_BIND"},              {0x1000, "APPCONTAINER"},
        {0x2000, "WDM_DRIVER"},           {0x4000, "GUARD_CF (CFG)"},
        {0x8000, "TERMINAL_SERVER_AWARE"},
    };
    QStringList out;
    for (const auto &f : flags)
        if (c & f.bit)
            out << QLatin1String(f.name);
    return out;
}

QString timeStampText(uint32_t t)
{
    if (t == 0)
        return QStringLiteral("0 (N/A)");
    return QStringLiteral("%1  ·  %2 UTC")
        .arg(hex(t, 8),
             QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::utc())
                 .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

// ---- HTML builders -------------------------------------------------------

QString chip(const QString &text, const QString &bg, const QString &fg)
{
    return QStringLiteral(
               "<td style='background-color:%1; color:%2; padding:3px 9px;'>"
               "<b>%3</b></td><td>&nbsp;&nbsp;</td>")
        .arg(bg, fg, esc(text));
}

class HtmlBuilder {
public:
    void section(const QString &title)
    {
        m_html += QStringLiteral(
                      "<p style='color:%1; font-size:12px; font-weight:bold; "
                      "margin:16px 0 2px 0;'>%2</p>"
                      "<table width='100%%' cellspacing='0' cellpadding='4' "
                      "style='background-color:%3; margin:0;'>")
                      .arg(kColSection, esc(title), kColCardBg);
        m_open = true;
    }

    // key/value row; mono=true renders the value in a monospace, blue tone
    void kv(const QString &key, const QString &value, bool mono = false,
            const QString &valueColor = {})
    {
        const QString color = valueColor.isEmpty()
                                  ? (mono ? kColNumber : kColValue)
                                  : valueColor;
        const QString font = mono ? QStringLiteral("font-family:Consolas,monospace;")
                                  : QString();
        m_html += QStringLiteral(
                      "<tr>"
                      "<td width='38%%' style='color:%1; vertical-align:top;'>%2</td>"
                      "<td style='color:%3; %4'>%5</td>"
                      "</tr>")
                      .arg(kColLabel, esc(key), color, font, esc(value));
    }

    void endSection() { if (m_open) { m_html += QStringLiteral("</table>"); m_open = false; } }

    // chips placed right after the current section, as inline spans inside a
    // paragraph so they wrap to the next line instead of being clipped
    void chips(const QStringList &flags)
    {
        endSection();
        if (flags.isEmpty())
            return;
        QString spans;
        for (const QString &f : flags)
            spans += QStringLiteral(
                         "<span style='background-color:%1; color:%2;'>"
                         "&nbsp;%3&nbsp;</span>&nbsp;&nbsp;")
                         .arg(kColChipBg, kColChipTx, esc(f));
        m_html += QStringLiteral(
                      "<p style='margin:4px 0 6px 0; line-height:190%%;'>%1</p>")
                      .arg(spans);
    }

    QString html() const { return m_html; }

private:
    QString m_html;
    bool m_open = false;
};

} // namespace

QString HeaderInfoDialog::formatHeaderInfo(const core::PeInfo &pe)
{
    // plain-text version used by the Copy button
    QStringList s;
    const auto kv = [&s](const QString &k, const QString &v) {
        s << QStringLiteral("%1 : %2").arg(k, -22).arg(v);
    };
    const auto sec = [&s](const QString &t) {
        s << QString() << QStringLiteral("== %1 ==").arg(t);
    };

    sec(QObject::tr("File"));
    kv(QObject::tr("Path"), QDir::toNativeSeparators(pe.filePath));
    kv(QObject::tr("Size"), QStringLiteral("%1 bytes").arg(QLocale::c().toString(pe.fileSize)));
    if (pe.fileTime.isValid())
        kv(QObject::tr("Modified"), pe.fileTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!pe.fileAttributes.isEmpty())
        kv(QObject::tr("Attributes"), pe.fileAttributes);
    if (!pe.fileVer.isEmpty())
        kv(QObject::tr("File Version"), pe.fileVer);
    if (!pe.productVer.isEmpty())
        kv(QObject::tr("Product Version"), pe.productVer);

    sec(QObject::tr("COFF Header"));
    kv(QObject::tr("Machine"), QStringLiteral("%1  (%2)").arg(machineName(pe.machine), hex(pe.machine, 4)));
    kv(QObject::tr("Format"), pe.is64 ? QStringLiteral("PE32+ (64-bit)") : QStringLiteral("PE32 (32-bit)"));
    kv(QObject::tr("Link Time Stamp"), timeStampText(pe.linkTimeStamp));
    kv(QObject::tr("Characteristics"),
       QStringLiteral("%1  [%2]").arg(hex(pe.characteristics, 4),
                                      characteristicFlags(pe.characteristics).join(QLatin1String(", "))));

    sec(QObject::tr("Optional Header"));
    kv(QObject::tr("Subsystem"), core::subsystemName(pe.subsystem));
    kv(QObject::tr("Preferred Base"), hex(pe.preferredBase, pe.is64 ? 16 : 8));
    kv(QObject::tr("Size of Image"), hex(pe.sizeOfImage, 8));
    kv(QObject::tr("Linker Version"), pe.linkerVer);
    kv(QObject::tr("OS Version"), pe.osVer);
    kv(QObject::tr("Image Version"), pe.imageVer);
    kv(QObject::tr("Subsystem Version"), pe.subsystemVer);
    kv(QObject::tr("Link Checksum"), hex(pe.linkChecksum, 8));
    kv(QObject::tr("Real Checksum"),
       QStringLiteral("%1  %2").arg(hex(pe.realChecksum, 8),
                                    (pe.linkChecksum == 0 || pe.linkChecksum == pe.realChecksum)
                                        ? QObject::tr("(OK)") : QObject::tr("(MISMATCH)")));
    kv(QObject::tr("DLL Characteristics"),
       QStringLiteral("%1  [%2]").arg(hex(pe.dllCharacteristics, 4),
                                      dllCharacteristicFlags(pe.dllCharacteristics).join(QLatin1String(", "))));

    sec(QObject::tr("Contents"));
    kv(QObject::tr("Image Type"), pe.isDll() ? QStringLiteral("DLL") : QStringLiteral("EXE"));
    kv(QObject::tr("Managed (.NET)"), pe.isDotNet ? QObject::tr("Yes") : QObject::tr("No"));
    kv(QObject::tr("Thread Local Storage"), pe.hasTls ? QObject::tr("Yes") : QObject::tr("No"));
    kv(QObject::tr("Debug Symbols"), pe.symbols);
    kv(QObject::tr("Embedded Manifest"), pe.manifestXml.isEmpty() ? QObject::tr("No") : QObject::tr("Yes"));
    kv(QObject::tr("Export Count"), QString::number(pe.exports.size()));
    int importedFuncs = 0;
    for (const auto &m : pe.imports)
        importedFuncs += int(m.funcs.size());
    kv(QObject::tr("Import Modules"), QString::number(pe.imports.size()));
    kv(QObject::tr("Imported Functions"), QString::number(importedFuncs));

    return s.join(QLatin1Char('\n')).trimmed();
}

namespace {

QString buildHtml(const core::PeInfo &pe)
{
    const QString name = QFileInfo(pe.filePath).fileName();

    // identity badges
    QString badges;
    badges += chip(pe.is64 ? QStringLiteral("x64") : QStringLiteral("x86"),
                   QStringLiteral("#0e3a5c"), QStringLiteral("#9cdcfe"));
    badges += chip(pe.is64 ? QStringLiteral("PE32+") : QStringLiteral("PE32"),
                   QStringLiteral("#3a2f5c"), QStringLiteral("#c5b4f0"));
    badges += chip(pe.isDll() ? QStringLiteral("DLL") : QStringLiteral("EXE"),
                   QStringLiteral("#103a32"), QStringLiteral("#4ec9b0"));
    badges += chip(core::subsystemName(pe.subsystem),
                   QStringLiteral("#3a3320"), QStringLiteral("#dcdcaa"));
    if (pe.isDotNet)
        badges += chip(QStringLiteral(".NET"), QStringLiteral("#4a2a3a"),
                       QStringLiteral("#f0a0c0"));

    HtmlBuilder b;

    // header banner
    QString html =
        QStringLiteral(
            "<div style='margin-bottom:6px;'>"
            "<span style='color:#d4d4d4; font-size:17px; font-weight:bold;'>%1</span><br/>"
            "<span style='color:#858585; font-family:Consolas,monospace;'>%2</span>"
            "</div>"
            "<table cellspacing='3' cellpadding='0' style='margin:6px 0 4px 0;'><tr>%3</tr></table>")
            .arg(esc(name), esc(QDir::toNativeSeparators(pe.filePath)), badges);

    b.section(QObject::tr("File"));
    b.kv(QObject::tr("Size"),
         QStringLiteral("%1 bytes").arg(QLocale::c().toString(pe.fileSize)), true);
    if (pe.fileTime.isValid())
        b.kv(QObject::tr("Modified"),
             pe.fileTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), true);
    if (!pe.fileAttributes.isEmpty())
        b.kv(QObject::tr("Attributes"), pe.fileAttributes);
    if (!pe.fileVer.isEmpty())
        b.kv(QObject::tr("File Version"), pe.fileVer, true);
    if (!pe.productVer.isEmpty())
        b.kv(QObject::tr("Product Version"), pe.productVer, true);
    b.endSection();

    b.section(QObject::tr("COFF Header"));
    b.kv(QObject::tr("Machine"),
         QStringLiteral("%1  (%2)").arg(machineName(pe.machine), hex(pe.machine, 4)));
    b.kv(QObject::tr("Format"),
         pe.is64 ? QStringLiteral("PE32+ (64-bit)") : QStringLiteral("PE32 (32-bit)"));
    b.kv(QObject::tr("Link Time Stamp"), timeStampText(pe.linkTimeStamp), true);
    b.kv(QObject::tr("Characteristics"), hex(pe.characteristics, 4), true);
    b.chips(characteristicFlags(pe.characteristics));

    b.section(QObject::tr("Optional Header"));
    b.kv(QObject::tr("Subsystem"), core::subsystemName(pe.subsystem));
    b.kv(QObject::tr("Preferred Base"), hex(pe.preferredBase, pe.is64 ? 16 : 8), true);
    b.kv(QObject::tr("Size of Image"), hex(pe.sizeOfImage, 8), true);
    b.kv(QObject::tr("Linker Version"), pe.linkerVer, true);
    b.kv(QObject::tr("OS Version"), pe.osVer, true);
    b.kv(QObject::tr("Image Version"), pe.imageVer, true);
    b.kv(QObject::tr("Subsystem Version"), pe.subsystemVer, true);
    b.kv(QObject::tr("Link Checksum"), hex(pe.linkChecksum, 8), true);
    const bool checksumOk = pe.linkChecksum == 0 || pe.linkChecksum == pe.realChecksum;
    b.kv(QObject::tr("Real Checksum"),
         QStringLiteral("%1  %2").arg(hex(pe.realChecksum, 8),
                                      checksumOk ? QObject::tr("(OK)") : QObject::tr("(MISMATCH)")),
         true, checksumOk ? kColOk : kColWarn);
    b.kv(QObject::tr("DLL Characteristics"), hex(pe.dllCharacteristics, 4), true);
    b.chips(dllCharacteristicFlags(pe.dllCharacteristics));

    b.section(QObject::tr("Contents"));
    b.kv(QObject::tr("Image Type"), pe.isDll() ? QStringLiteral("DLL") : QStringLiteral("EXE"));
    b.kv(QObject::tr("Managed (.NET)"), pe.isDotNet ? QObject::tr("Yes") : QObject::tr("No"));
    b.kv(QObject::tr("Thread Local Storage"), pe.hasTls ? QObject::tr("Yes") : QObject::tr("No"));
    b.kv(QObject::tr("Debug Symbols"), pe.symbols);
    b.kv(QObject::tr("Embedded Manifest"),
         pe.manifestXml.isEmpty() ? QObject::tr("No") : QObject::tr("Yes"));
    b.kv(QObject::tr("Export Count"), QString::number(pe.exports.size()), true);
    int importedFuncs = 0;
    for (const auto &m : pe.imports)
        importedFuncs += int(m.funcs.size());
    b.kv(QObject::tr("Import Modules"), QString::number(pe.imports.size()), true);
    b.kv(QObject::tr("Imported Functions"), QString::number(importedFuncs), true);
    b.endSection();

    return QStringLiteral("<div style='margin:4px 6px;'>%1%2</div>").arg(html, b.html());
}

} // namespace

HeaderInfoDialog::HeaderInfoDialog(const core::PeInfoPtr &pe, QWidget *parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Header Information — %1").arg(QFileInfo(pe->filePath).fileName()));
    resize(560, 640);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    auto *view = new QTextBrowser(this);
    view->setOpenExternalLinks(false);
    view->setStyleSheet(QStringLiteral("QTextBrowser{background-color:#1e1e1e; border:none;}"));
    QFont uiFont(QStringLiteral("Segoe UI"));
    uiFont.setPointSizeF(9.5);
    view->setFont(uiFont);
    view->setHtml(buildHtml(*pe));
    layout->addWidget(view);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *copyBtn = new QPushButton(tr("Copy"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    buttons->addWidget(copyBtn);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(copyBtn, &QPushButton::clicked, this, [pe]() {
        QApplication::clipboard()->setText(formatHeaderInfo(*pe));
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace ui
