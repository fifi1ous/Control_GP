#include "firstrunsetup.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryFile>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char *kTitle = "Control_GP";

QDir appDir()
{
    return QDir(QCoreApplication::applicationDirPath());
}

QString setupScriptPath()
{
    return appDir().filePath("tools/setup_deps.ps1");
}

QString nativeAppDir()
{
    return QDir::toNativeSeparators(appDir().absolutePath());
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = f.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))  // PowerShell 5.1 writes a UTF-8 BOM
        data.remove(0, 3);
    return QJsonDocument::fromJson(data).object();
}

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------

// setup_deps.ps1 stores the SHA256 of the requirements.txt it installed in
// python-embed/.installed.json. A new requirements.txt (new release) therefore
// triggers the download again.
bool pythonPackagesReady()
{
    QFile req(appDir().filePath("requirements.txt"));
    if (!req.open(QIODevice::ReadOnly))
        return false;
    const QByteArray expected =
        QCryptographicHash::hash(req.readAll(), QCryptographicHash::Sha256).toHex().toUpper();

    const QJsonObject stamp = readJson(appDir().filePath("python-embed/.installed.json"));
    return stamp.value("requirements").toString().toLatin1().toUpper() == expected;
}

bool isPopplerBin(const QString &dir)
{
    return QFileInfo::exists(dir + "/pdftoppm.exe") && QFileInfo::exists(dir + "/pdfinfo.exe");
}

// Returns the folder with pdftoppm.exe and pdfinfo.exe, or an empty string.
// Standard place is poppler/Library/bin (setup_deps.ps1). After a manual
// download the user may unzip it elsewhere in the app folder, e.g.
// poppler-24.08.0/Library/bin, so the other subfolders are searched too.
QString findPopplerBin()
{
    const QString standard = appDir().filePath("poppler/Library/bin");
    if (isPopplerBin(standard))
        return standard;

    static const QStringList skip = { "python-embed", "workspace", "scripts", "models",
                                      "database", "tessdata", "tools", "licenses" };
    const QFileInfoList top = appDir().entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &d : top) {
        if (skip.contains(d.fileName(), Qt::CaseInsensitive))
            continue;
        QDirIterator it(d.absoluteFilePath(), { "pdftoppm.exe" }, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString bin = QFileInfo(it.next()).absolutePath();
            if (isPopplerBin(bin))
                return bin;
        }
    }
    return {};
}

bool appDirWritable()
{
    QTemporaryFile probe(appDir().filePath("write-test-XXXXXX.tmp"));
    return probe.open();
}

// ---------------------------------------------------------------------------
// Running setup_deps.ps1
// ---------------------------------------------------------------------------

// Runs setup_deps.ps1 with the given arguments in a modal dialog that shows
// its output. Returns true when the script finished with exit code 0.
bool runSetupScript(const QStringList &scriptArgs, const QString &what)
{
    QDialog dlg;
    dlg.setWindowTitle(QStringLiteral("Control_GP – stahování: %1").arg(what));
    dlg.resize(760, 460);

    auto *layout = new QVBoxLayout(&dlg);
    auto *info = new QLabel(QStringLiteral(
        "Stahuji %1. Podle rychlosti připojení to může trvat několik minut.").arg(what));
    info->setWordWrap(true);
    auto *progress = new QProgressBar;
    progress->setRange(0, 0);  // indeterminate while the script runs
    auto *log = new QPlainTextEdit;
    log->setReadOnly(true);
    log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Close)->setEnabled(false);

    layout->addWidget(info);
    layout->addWidget(progress);
    layout->addWidget(log, 1);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setWorkingDirectory(appDir().absolutePath());

    bool ok = false;

    QObject::connect(&proc, &QProcess::readyReadStandardOutput, &dlg, [&]() {
        log->appendPlainText(QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed());
    });

    QObject::connect(&proc, &QProcess::finished, &dlg,
                     [&](int exitCode, QProcess::ExitStatus status) {
        ok = (status == QProcess::NormalExit && exitCode == 0);
        progress->setRange(0, 1);
        progress->setValue(ok ? 1 : 0);
        info->setText(ok
            ? QStringLiteral("Hotovo, můžete okno zavřít.")
            : QStringLiteral("Stahování se nepodařilo. Výpis výše ukazuje, kde nastala chyba. "
                             "Zkontrolujte připojení k internetu a spusťte aplikaci znovu."));
        buttons->button(QDialogButtonBox::Close)->setEnabled(true);
    });

    QObject::connect(&proc, &QProcess::errorOccurred, &dlg, [&](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart)
            return;
        info->setText(QStringLiteral("Nepodařilo se spustit PowerShell. Spusťte prosím Setup.cmd ručně."));
        progress->setRange(0, 1);
        progress->setValue(0);
        buttons->button(QDialogButtonBox::Close)->setEnabled(true);
    });

    QStringList args = { "-NoProfile", "-ExecutionPolicy", "Bypass",
                         "-File", QDir::toNativeSeparators(setupScriptPath()) };
    args << scriptArgs;
    proc.start(QStringLiteral("powershell.exe"), args);

    dlg.exec();

    // Closed while still running (window close button): stop the script.
    if (proc.state() != QProcess::NotRunning) {
        proc.kill();
        proc.waitForFinished(5000);
        return false;
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Python packages
// ---------------------------------------------------------------------------

bool ensurePythonPackages()
{
    if (pythonPackagesReady())
        return true;

    const auto answer = QMessageBox::question(nullptr, kTitle,
        QStringLiteral("Aplikace potřebuje Python knihovny (onnxruntime, OpenCV, numpy a další), "
                       "které zatím nejsou stažené. Stažení má řádově 100 MB a proběhne jen jednou.\n\n"
                       "Stáhnout knihovny nyní?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        QMessageBox::information(nullptr, kTitle,
            QStringLiteral("Bez Python knihoven nelze aplikaci spustit. "
                           "Při příštím spuštění se aplikace zeptá znovu."));
        return false;
    }

    if (runSetupScript({ "-Only", "python" }, QStringLiteral("Python knihovny")) && pythonPackagesReady())
        return true;

    QMessageBox::critical(nullptr, kTitle,
        QStringLiteral("Python knihovny se nepodařilo připravit. Spusťte aplikaci znovu, "
                       "případně spusťte Setup.cmd ve složce aplikace a sledujte výpis."));
    return false;
}

// ---------------------------------------------------------------------------
// Poppler
// ---------------------------------------------------------------------------

// Download URL and version of the Poppler build listed in deps.lock.
void popplerFromLock(QString *url, QString *version)
{
    const QJsonObject poppler = readJson(appDir().filePath("deps.lock")).value("poppler").toObject();
    *url = poppler.value("url").toString();
    *version = poppler.value("version").toString();
}

// Manual download: open the ZIP URL in the browser and wait until the user
// unzips it into the app folder. Returns true once Poppler is found.
bool manualPopplerDownload()
{
    QString url, version;
    popplerFromLock(&url, &version);
    if (url.isEmpty())
        url = QStringLiteral("https://github.com/oschwartz10612/poppler-windows/releases");
    QDesktopServices::openUrl(QUrl(url));

    for (;;) {
        QMessageBox box;
        box.setWindowTitle(kTitle);
        box.setIcon(QMessageBox::Information);
        box.setTextFormat(Qt::RichText);
        box.setTextInteractionFlags(Qt::TextBrowserInteraction);
        box.setText(QStringLiteral(
            "<p>V prohlížeči se stahuje Poppler%1. Pokud stahování nezačalo, otevřete odkaz:<br>"
            "<a href=\"%2\">%2</a></p>"
            "<p>Stažený ZIP rozbalte do složky aplikace:<br><b>%3</b></p>"
            "<p>Po rozbalení klikněte na <b>Zkontrolovat</b>.</p>")
            .arg(version.isEmpty() ? QString() : QStringLiteral(" (verze %1)").arg(version.toHtmlEscaped()),
                 url.toHtmlEscaped(), nativeAppDir().toHtmlEscaped()));
        QPushButton *check = box.addButton(QStringLiteral("Zkontrolovat"), QMessageBox::AcceptRole);
        QPushButton *folder = box.addButton(QStringLiteral("Otevřít složku aplikace"), QMessageBox::ActionRole);
        box.addButton(QStringLiteral("Zrušit"), QMessageBox::RejectRole);
        box.setDefaultButton(check);
        box.exec();

        if (box.clickedButton() == folder) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(appDir().absolutePath()));
            continue;
        }
        if (box.clickedButton() != check)
            return false;
        if (!findPopplerBin().isEmpty())
            return true;

        QMessageBox::warning(nullptr, kTitle,
            QStringLiteral("Poppler ve složce aplikace zatím není. Hledám soubory pdftoppm.exe "
                           "a pdfinfo.exe ve složce Library\\bin uvnitř rozbaleného ZIPu.\n\n"
                           "Zkontrolujte, že je ZIP rozbalený do:\n%1").arg(nativeAppDir()));
    }
}

bool ensurePoppler()
{
    if (!findPopplerBin().isEmpty())
        return true;

    QMessageBox box;
    box.setWindowTitle(kTitle);
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral(
        "Aplikace potřebuje Poppler (nástroj pro převod PDF na obrázky), který zatím není stažený.\n\n"
        "Doporučujeme automatické stažení. Při ručním stažení se otevře odkaz v prohlížeči "
        "a ZIP rozbalíte do složky aplikace sami."));
    QPushButton *autoBtn = box.addButton(QStringLiteral("Stáhnout automaticky"), QMessageBox::AcceptRole);
    QPushButton *manualBtn = box.addButton(QStringLiteral("Stáhnout ručně"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("Zrušit"), QMessageBox::RejectRole);
    box.setDefaultButton(autoBtn);
    box.exec();

    bool ok = false;
    if (box.clickedButton() == autoBtn) {
        ok = runSetupScript({ "-Only", "poppler" }, QStringLiteral("Poppler"))
             && !findPopplerBin().isEmpty();
        if (!ok)
            QMessageBox::critical(nullptr, kTitle,
                QStringLiteral("Poppler se nepodařilo stáhnout. Spusťte aplikaci znovu "
                               "a zkuste ruční stažení."));
    } else if (box.clickedButton() == manualBtn) {
        ok = manualPopplerDownload();
    }

    if (!ok)
        QMessageBox::information(nullptr, kTitle,
            QStringLiteral("Bez Poppleru nelze aplikaci spustit. "
                           "Při příštím spuštění se aplikace zeptá znovu."));
    return ok;
}

// The Python scripts read the Poppler folder from this variable.
void exportPopplerPath()
{
    const QString bin = findPopplerBin();
    if (!bin.isEmpty())
        qputenv("CONTROL_GP_POPPLER_BIN", QDir::toNativeSeparators(bin).toUtf8());
}

} // namespace

bool FirstRunSetup::ensureDependencies()
{
    // Developer build: dependencies are synced next to the exe by CMake.
    if (!QFileInfo::exists(setupScriptPath())) {
        exportPopplerPath();
        return true;
    }

    const bool needPython  = !pythonPackagesReady();
    const bool needPoppler = findPopplerBin().isEmpty();

    if ((needPython || needPoppler) && !appDirWritable()) {
        QMessageBox::critical(nullptr, kTitle,
            QStringLiteral("Do složky aplikace nelze zapisovat:\n%1\n\n"
                           "Rozbalte Control_GP do složky, kam máte právo zápisu, "
                           "například do Dokumentů, a spusťte ho znovu.").arg(nativeAppDir()));
        return false;
    }

    if (needPython && !ensurePythonPackages())
        return false;
    if (needPoppler && !ensurePoppler())
        return false;

    exportPopplerPath();
    return true;
}
