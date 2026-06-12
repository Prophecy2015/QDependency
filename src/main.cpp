#include "ui/mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QStyleFactory>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("QDepends"));
    QApplication::setApplicationName(QStringLiteral("QDepends"));
    QApplication::setApplicationVersion(QStringLiteral("1.1.1"));

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // VS Code Dark+ palette as the base, refined by the stylesheet
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::WindowText, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::AlternateBase, QColor(0x25, 0x25, 0x26));
    palette.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Button, QColor(0x2d, 0x2d, 0x2d));
    palette.setColor(QPalette::ButtonText, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0x25, 0x25, 0x26));
    palette.setColor(QPalette::ToolTipText, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
    palette.setColor(QPalette::PlaceholderText, QColor(0x85, 0x85, 0x85));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6e, 0x6e, 0x6e));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6e, 0x6e, 0x6e));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6e, 0x6e, 0x6e));
    app.setPalette(palette);

    QFile qss(QStringLiteral(":/resources/theme/dark.qss"));
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    // translations (English base, Chinese language pack)
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("qdepends"), QStringLiteral("_"),
                           QStringLiteral(":/i18n")))
        app.installTranslator(&appTranslator);

    ui::MainWindow window;
    window.show();

    const QStringList args = app.arguments();
    if (args.size() > 1)
        window.openFile(args.at(1));

    return app.exec();
}
