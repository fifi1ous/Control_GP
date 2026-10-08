#include "displayresults.h"
#include "maps.h"
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <functional>
#include <initializer_list>

DisplayResults::DisplayResults() {}

namespace {

QString fitToWidth(const QString& s, int width)
{
    if (width <= 0)
        return QString();
    if (s.length() <= width)
        return s.leftJustified(width);
    return s.left(width - 1) + QStringLiteral("…");
}

// Find the NAMESMAP key whose token best matches the filename. The longest
// matching key wins so e.g. "popispole" is preferred over a hypothetical
// shorter prefix that also appears in the name.
QString findExpectedKey(const QString& fileName)
{
    QString bestKey;
    int bestLen = 0;
    for (auto it = NAMESMAP.begin(); it != NAMESMAP.end(); ++it) {
        if (fileName.contains(it.key(), Qt::CaseInsensitive) && it.key().length() > bestLen) {
            bestLen = it.key().length();
            bestKey = it.key();
        }
    }
    return bestKey;
}

QString labelFor(const QString& key)
{
    auto it = NAMESMAP.find(key);
    return (it == NAMESMAP.end()) ? key : it.value();
}

} // namespace


QString DisplayResults::generateReport(const std::vector<QString>& fileNames,
                                       const std::vector<QString>& predictions,
                                       bool hasGnss)
{
    const size_t rowCount = (std::min)(fileNames.size(), predictions.size());

    struct Row {
        QString expectedLabel;
        QString fileName;
        QString predictedLabel;
        bool     hasExpected;
        bool     match;
    };

    std::vector<Row> rows;
    rows.reserve(rowCount);

    int correctCount = 0;
    for (size_t i = 0; i < rowCount; ++i) {
        const QString& fileName   = fileNames[i];
        const QString& prediction = predictions[i];

        const QString expectedKey = findExpectedKey(fileName);
        const bool hasExpected    = !expectedKey.isEmpty();
        // Compare keys (short codes), not localized labels.
        const bool match = hasExpected && expectedKey.compare(prediction, Qt::CaseInsensitive) == 0;
        if (match)
            ++correctCount;

        rows.push_back({
            hasExpected ? labelFor(expectedKey) : QStringLiteral("—"),
            fileName,
            labelFor(prediction),
            hasExpected,
            match
        });
    }

    const QString h1 = QStringLiteral("Soubor");
    const QString h2 = QStringLiteral("Název souboru");
    const QString h3 = QStringLiteral("Predikce třídy");
    const QString h4 = QStringLiteral("Správně");

    // Hardcoded column widths — every cell is forced to exactly these lengths
    // by fitToWidth (padded if shorter, truncated with … if longer).
    constexpr int w1 = 21;  // "Soubor"          — fits "Protokol o výpočtech"
    constexpr int w2 = 42;  // "Název souboru"   — fits "641146_ZPMZ_040111_popispole_A_signed.pdf"
    constexpr int w3 = 21;  // "Predikce třídy"  — fits "Protokol o výpočtech"
    constexpr int w4 = 9;   // "Správně"

    // Outer cell width = content width + a single space of padding on each side.
    const int c1 = w1 + 2;
    const int c2 = w2 + 2;
    const int c3 = w3 + 2;
    const int c4 = w4 + 2;

    QString result;
    QTextStream out(&result);

    auto writeRow = [&](const QString& a, const QString& b, const QString& c, const QString& d) {
        out << "| " << fitToWidth(a, w1) << " "
            << "| " << fitToWidth(b, w2) << " "
            << "| " << fitToWidth(c, w3) << " "
            << "| " << fitToWidth(d, w4) << " |\n";
    };

    auto writeSeparator = [&](QChar ch) {
        out << "+" << QString(c1, ch)
            << "+" << QString(c2, ch)
            << "+" << QString(c3, ch)
            << "+" << QString(c4, ch)
            << "+\n";
    };

    writeSeparator('=');
    writeRow(h1, h2, h3, h4);
    writeSeparator('=');
    for (const auto& r : rows) {
        const QString matchText = !r.hasExpected ? QStringLiteral("?")
                                                 : (r.match ? QStringLiteral("✓") : QStringLiteral("✗"));
        writeRow(r.expectedLabel, r.fileName, r.predictedLabel, matchText);
    }
    writeSeparator('=');

    out << "\n";
    out << "Správně klasifikováno: " << correctCount << " / " << rowCount << "\n";
    out << "Bylo měřeno pomocí GNSS: " << (hasGnss ? "Ano" : "Ne") << "\n";

    return result;
}


QString DisplayResults::generatePdfReport(const PdfControlReportFormat& report)
{
    // Status → glyph shown in the rightmost column.
    auto glyphFor = [](CheckStatus s) -> QString {
        switch (s) {
        case CheckStatus::Ok:      return QStringLiteral("✓");
        case CheckStatus::Error:   return QStringLiteral("✗");
        case CheckStatus::Info:    return QStringLiteral("·");
        }
        return QStringLiteral("·");
    };

    constexpr int w1 = 18;  // "Kontrola"
    constexpr int w2 = 28;  // "Hodnota"
    constexpr int w3 = 4;   // status glyph
    constexpr int w4 = 36;  // "Poznámka"

    const int c1 = w1 + 2;
    const int c2 = w2 + 2;
    const int c3 = w3 + 2;
    const int c4 = w4 + 2;

    QString result;
    QTextStream out(&result);

    auto writeRow = [&](const QString& a, const QString& b, const QString& c, const QString& d) {
        out << "| " << fitToWidth(a, w1) << " "
            << "| " << fitToWidth(b, w2) << " "
            << "| " << fitToWidth(c, w3) << " "
            << "| " << fitToWidth(d, w4) << " |\n";
    };

    auto writeSeparator = [&](QChar ch) {
        out << "+" << QString(c1, ch)
            << "+" << QString(c2, ch)
            << "+" << QString(c3, ch)
            << "+" << QString(c4, ch)
            << "+\n";
    };

    out << "Kontrola souboru " << report.fileName
        << " --- na základě klasifikace byl soubor určen jako --- "
        << (report.category.isEmpty() ? QStringLiteral("—") : report.category)
        << "\n";

    writeSeparator('=');
    writeRow(QStringLiteral("Kontrola"), QStringLiteral("Hodnota"),
             QStringLiteral(" "), QStringLiteral("Poznámka"));
    writeSeparator('=');

    int errors = 0;
    for (const CheckItem& item : report.checks) {
        if (item.status == CheckStatus::Error)   ++errors;
        writeRow(item.label, item.value, glyphFor(item.status), item.note);
    }
    writeSeparator('=');

    return result;
}


QString DisplayResults::generatePdfReport(const std::vector<PdfControlReportFormat>& reports)
{
    if (reports.empty())
        return QStringLiteral("Žádné soubory ke kontrole.\n");

    QString result;
    QTextStream out(&result);
    for (const PdfControlReportFormat& report : reports) {
        out << generatePdfReport(report);
    }
    return result;
}


QString DisplayResults::generateNonPdfReport(const NonPdfControlReport& report)
{
    QString result;
    QTextStream out(&result);

    out << "Kontrola ostatních souborů\n";

    // --- VFK ---
    if (report.vfkPresent)
        out << "VFK: přítomen — " << report.vfkName
            << "-------- formát souboru:" << report.vfkFormat << "\n";
    else
        out << "VFK: nepřítomen\n";

    if (report.ssPresent)
        out << "SS: přítomen — " << report.ssName
            << "-------- formát souboru:" << report.ssFormat << "\n";
    else
        out << "SS: nepřítomen\n";

    // --- Signatures (AZI1 signed files, AZI2 signature, AZI3 time stamp) ---
    for (const SignatureEntry& s : report.signatures) {
        if (s.present)
            out << s.label << ": přítomen — " << s.name
                << "-------- formát souboru:" << s.format << ")\n";
        else
            out << s.label << ": nepřítomen\n";
    }

    // --- GNSS ---
    out << "Bylo měřeno pomocí GNSS: "
        << (report.gnssUsed ? "Ano" : "Ne") << "\n";

    // --- Signature verification (Ověření) ---
    if (report.overeniPresent)
        out << "Podpisy byly ověřeny pomocí webové služby, výsledky v souboru "
            << report.overeniName << "\n";
    else
        out << "Podpisy nebyly ověřeny pomocí webové služby\n";

    // --- Other files (PDFs also show their predicted class) ---
    out << "\nOstatní soubory:\n";
    if (report.others.empty()) {
        out << "—\n";
    } else {
        for (const OtherFileEntry& o : report.others) {
            out << "  " << o.name << "-------- formát souboru:" << o.format << ")";
            if (o.isPdf)
                out << "Na základě klasifikace soubor s největší pravděpodobností obsahuje: "
                    << (o.documentClass.isEmpty() ? QStringLiteral("?") : o.documentClass);
            out << "\n";
        }
    }

    return result;
}


QString DisplayResults::generateSSReport(const SSControlReport& report)
{
    // Status → glyph shown next to each point's heading.
    auto glyphFor = [](CheckStatus s) -> QString {
        switch (s) {
        case CheckStatus::Ok:    return QStringLiteral("✓");
        case CheckStatus::Error: return QStringLiteral("✗");
        case CheckStatus::Info:  return QStringLiteral("·");
        }
        return QStringLiteral("·");
    };

    // A numeric coordinate with two decimals, or an em dash when the source
    // either lacks the point entirely or simply does not carry the field.
    auto fmtNum = [](bool present, bool has, double v) -> QString {
        if (!present || !has) return QStringLiteral("—");
        return QString::number(v, 'f', 2);
    };
    // A quality code as an integer, or an em dash when not carried.
    auto fmtKv = [](bool present, bool has, int v) -> QString {
        if (!present || !has) return QStringLiteral("—");
        return QString::number(v);
    };
    // A free-text description, or an em dash when the source has none.
    auto fmtDesc = [](const SSPointValues& s) -> QString {
        if (!s.present || !s.hasDesc()) return QStringLiteral("—");
        return s.description;
    };

    // Field selectors, shared by the source rows and the difference row.
    auto always   = [](const SSPointValues&)   { return true; };
    auto hasPair2 = [](const SSPointValues& s) { return s.hasPair2(); };
    auto hasKv1   = [](const SSPointValues& s) { return s.hasKv1(); };
    auto hasKv2   = [](const SSPointValues& s) { return s.hasKv2(); };

    // Spread (max − min) of a field across the sources that are present and
    // actually carry it; @p count is set to how many sources contributed.
    auto spread = [](const SSPointComparison& p,
                     const std::function<bool(const SSPointValues&)>& has,
                     const std::function<double(const SSPointValues&)>& get,
                     int& count) -> double {
        double mn = 0, mx = 0; int n = 0;
        for (const SSPointValues* s : { &p.gp, &p.vfk, &p.prot }) {
            if (!s->present || !has(*s)) continue;
            const double v = get(*s);
            if (n == 0) { mn = mx = v; }
            else        { mn = std::min(mn, v); mx = std::max(mx, v); }
            ++n;
        }
        count = n;
        return mx - mn;
    };

    // Difference cell for a coordinate: the numeric spread, or an em dash when
    // fewer than two sources carry the field (nothing to compare).
    auto diffCoord = [&](const SSPointComparison& p,
                         const std::function<bool(const SSPointValues&)>& has,
                         const std::function<double(const SSPointValues&)>& get) -> QString {
        int n = 0; const double d = spread(p, has, get, n);
        if (n < 2) return QStringLiteral("—");
        return QString::number(d, 'f', 2);
    };

    // Difference cell for a discrete field (quality code): "=" when the carried
    // values agree within tolerance, "≠" when they differ, "—" when < 2 carry it.
    auto diffMark = [&](const SSPointComparison& p,
                        const std::function<bool(const SSPointValues&)>& has,
                        const std::function<double(const SSPointValues&)>& get) -> QString {
        int n = 0; const double d = spread(p, has, get, n);
        if (n < 2) return QStringLiteral("—");
        return (d <= report.tolerance) ? QStringLiteral("=") : QStringLiteral("≠");
    };

    // Difference cell for the description: descriptions are only compared, not
    // required to match exactly, so this just states "shoda" / "liší".
    auto diffDesc = [](const SSPointComparison& p) -> QString {
        bool have = false; QString ref; int n = 0; bool differ = false;
        for (const SSPointValues* s : { &p.gp, &p.vfk, &p.prot }) {
            if (!s->present || !s->hasDesc()) continue;
            if (!have) { ref = s->description; have = true; }
            else if (ref != s->description) differ = true;
            ++n;
        }
        if (n < 2) return QStringLiteral("—");
        return differ ? QStringLiteral("liší") : QStringLiteral("shoda");
    };

    // Columns: soubor | Y | X | kv | Y | X | kv | popis. All columns are always
    // shown; the second Y/X/kv group holds the optional second coordinate pair.
    static const int widths[] = { 9, 12, 12, 4, 12, 12, 4, 18 };

    const QStringList header = {
        QStringLiteral("soubor"),
        QStringLiteral("Y"), QStringLiteral("X"), QStringLiteral("kk."),
        QStringLiteral("Y"), QStringLiteral("X"), QStringLiteral("kk."),
        QStringLiteral("popis"),
    };

    QString result;
    QTextStream out(&result);

    auto writeRow = [&](const QStringList& cells) {
        out << "|";
        for (int i = 0; i < cells.size(); ++i)
            out << " " << fitToWidth(cells[i], widths[i]) << " |";
        out << "\n";
    };

    auto writeSeparator = [&](QChar ch) {
        out << "+";
        for (int w : widths) out << QString(w + 2, ch) << "+";
        out << "\n";
    };

    out << "Kontrola souřadnic SS — porovnání GP, VFK a protokolu\n";

    // One warning for the whole report when the náčrt coordinates were not
    // available as a text block and had to be recovered with OCR.
    if (report.gpOcr) {
        out << "\n"
            << "⚠  Souřadnice geometrického plánu nebyly poskytnuty v textovém bloku;\n"
            << "   pro extrakci byl využit OCR Tesseract, který může obsahovat chyby.\n";
    }

    // One block per survey point: a heading with the verdict, then a four-row
    // table (gp, vfk, prot and the differences between them).
    for (const SSPointComparison& p : report.points) {
        // Points carried solely by the protocol (no gp and no VFK) are not
        // displayed — we never show coordinates that are only in the protocol.
        if (!p.gp.present && !p.vfk.present)
            continue;

        // Rows are keyed by the short point number; fall back to the full
        // number (cb_full) when a short number is missing.
        const QString label = (p.id != 0) ? QString::number(p.id)
                                          : QString::number(p.cbFull);

        out << "\nBod " << label;
        if (p.id != 0 && p.cbFull != 0)
            out << " (úplné číslo " << p.cbFull << ")";
        out << "   " << glyphFor(p.status) << " " << p.note << "\n";

        const bool gpOcr = p.gp.present && p.gp.ocr;
        if (gpOcr)
            out << "  ⚠  Souřadnice gp byly získány pomocí OCR (Tesseract) "
                   "– mohou obsahovat chyby.\n";

        writeSeparator('-');
        writeRow(header);
        writeSeparator('-');

        auto sourceRow = [&](const QString& name, const SSPointValues& s, bool ocrMark) {
            writeRow(QStringList{
                ocrMark ? name + QStringLiteral(" ⚠") : name,
                fmtNum(s.present, true,         s.Y1),
                fmtNum(s.present, true,         s.X1),
                fmtKv (s.present, s.hasKv1(),   s.kv1),
                fmtNum(s.present, s.hasPair2(), s.Y2),
                fmtNum(s.present, s.hasPair2(), s.X2),
                fmtKv (s.present, s.hasKv2(),   s.kv2),
                fmtDesc(s),
            });
        };
        sourceRow(QStringLiteral("gp"),   p.gp,   gpOcr);
        sourceRow(QStringLiteral("vfk"),  p.vfk,  false);
        sourceRow(QStringLiteral("prot"), p.prot, false);

        writeSeparator('-');
        writeRow(QStringList{
            QStringLiteral("Rozdíl"),
            diffCoord(p, always,   [](const SSPointValues& s) { return s.Y1; }),
            diffCoord(p, always,   [](const SSPointValues& s) { return s.X1; }),
            diffMark (p, hasKv1,   [](const SSPointValues& s) { return double(s.kv1); }),
            diffCoord(p, hasPair2, [](const SSPointValues& s) { return s.Y2; }),
            diffCoord(p, hasPair2, [](const SSPointValues& s) { return s.X2; }),
            diffMark (p, hasKv2,   [](const SSPointValues& s) { return double(s.kv2); }),
            diffDesc(p),
        });
        writeSeparator('-');
    }

    const int total = static_cast<int>(report.points.size());
    out << "\n";
    out << "Bodů celkem: "       << total             << "\n";
    out << "Souhlasí: "          << report.matched    << "\n";
    out << "Liší se: "           << report.mismatched << "\n";
    out << "Neúplné: "           << report.incomplete << "\n";
    out << "Pouze v protokolu: " << report.protOnly   << "\n";

    return result;
}


QString DisplayResults::generateVymeryReport(const VymeryControlReport& report)
{
    // Status → glyph shown next to each parcel's heading.
    auto glyphFor = [](CheckStatus s) -> QString {
        switch (s) {
        case CheckStatus::Ok:    return QStringLiteral("✓");
        case CheckStatus::Error: return QStringLiteral("✗");
        case CheckStatus::Info:  return QStringLiteral("·");
        }
        return QStringLiteral("·");
    };

    // Source key → human-readable label, taken from maps.h NAMESMAP where it
    // carries one. The VFK is not a NAMESMAP entry, so it is named explicitly.
    auto sourceLabel = [](const QString& key) -> QString {
        if (key == QStringLiteral("vfk")) return QStringLiteral("VFK");
        return labelFor(key);
    };

    // "st. 253" or "389/1": numbering type, base number and (optional) subdivision.
    auto parcelText = [](const QString& cislovani, int kmenove, int poddeleni) -> QString {
        QString s = cislovani.isEmpty() ? QString() : cislovani + QStringLiteral(" ");
        s += QString::number(kmenove);
        if (poddeleni != 0)
            s += QStringLiteral("/") + QString::number(poddeleni);
        return s;
    };

    // Columns: source | parcela | výměra | způsob určení | druh pozemku.
    static const int widths[] = { 22, 12, 9, 8, 22 };
    const QStringList header = {
        QStringLiteral("Soubor"),
        QStringLiteral("Parcela"),
        QStringLiteral("Výměra"),
        QStringLiteral("Zp.urč."),
        QStringLiteral("Druh pozemku"),
    };

    QString result;
    QTextStream out(&result);

    auto writeRow = [&](const QStringList& cells) {
        out << "|";
        for (int i = 0; i < cells.size(); ++i)
            out << " " << fitToWidth(cells[i], widths[i]) << " |";
        out << "\n";
    };

    auto writeSeparator = [&](QChar ch) {
        out << "+";
        for (int w : widths) out << QString(w + 2, ch) << "+";
        out << "\n";
    };

    // One source row: its values, or an em dash for a source that omitted the
    // parcel or simply does not carry a field (e.g. the protocol has no zp.urč.).
    auto sourceRow = [&](const QString& key, const VymeryValues& v) {
        if (!v.present) {
            writeRow(QStringList{ sourceLabel(key) + (v.ocr ? QStringLiteral(" ⚠") : QString()),
                                  QStringLiteral("—"), QStringLiteral("—"),
                                  QStringLiteral("—"), QStringLiteral("—") });
            return;
        }
        // The protocol carries a fractional area; show it unrounded. Other
        // sources work in whole m², so they print the integer value.
        const QString vymeraText = (v.vymera_d != 0.0)
            ? QString::number(v.vymera_d, 'f', 2)
            : QString::number(v.vymera);
        writeRow(QStringList{
            sourceLabel(key) + (v.ocr ? QStringLiteral(" ⚠") : QString()),
            parcelText(v.cislovani, v.kmenove, v.poddeleni),
            vymeraText,
            v.hasZpusUrc() ? QString::number(v.zpus_urc) : QStringLiteral("—"),
            v.hasDruhPoz() ? v.druh_poz                  : QStringLiteral("—"),
        });
    };

    // Render one state's parcels (old or new). The protocol row is only shown
    // for the new state, where it is carried.
    auto writeParcels = [&](const std::vector<VymeryComparison>& parcels) {
        for (const VymeryComparison& p : parcels) {
            out << "\nParcela "
                << parcelText(p.cislovani, p.kmenove, p.poddeleni)
                << "   " << glyphFor(p.status) << " " << p.note << "\n";

            writeSeparator('-');
            writeRow(header);
            writeSeparator('-');
            sourceRow(QStringLiteral("gp"),     p.gp);
            sourceRow(QStringLiteral("vfk"),    p.vfk);
            sourceRow(QStringLiteral("vymery"), p.vymery);
            if (p.withProt)
                sourceRow(QStringLiteral("prot"), p.prot);
            writeSeparator('-');
        }
    };

    out << "Kontrola výkazu výměr — porovnání GP, VFK, Výměr a protokolu\n";

    if (report.ocr) {
        out << "\n"
            << "⚠  Některé hodnoty nebyly poskytnuty v textovém bloku; pro extrakci\n"
            << "   byl využit OCR (Tesseract), který může obsahovat chyby.\n";
    }

    out << "\n=== Dosavadní stav ===\n";
    writeParcels(report.oldParcels);
    out << "\n  Souhlasí: "  << report.oldMatched
        << "   Liší se: "    << report.oldMismatched
        << "   Neúplné: "    << report.oldIncomplete << "\n";

    out << "\n=== Nový stav ===\n";
    writeParcels(report.newParcels);
    out << "\n  Souhlasí: "  << report.newMatched
        << "   Liší se: "    << report.newMismatched
        << "   Neúplné: "    << report.newIncomplete << "\n";

    // --- Closing balance: old total vs new total ---------------------------
    const int diff = report.sumNew - report.sumOld;
    out << "\n=== Porovnání dosavadního a nového stavu ===\n";
    out << "  Součet výměr – dosavadní stav: " << report.sumOld << " m²\n";
    out << "  Součet výměr – nový stav:      " << report.sumNew << " m²\n";
    out << "  Rozdíl (nový − dosavadní):     " << diff          << " m²\n";

    return result;
}


QString DisplayResults::generateSegmentationReport(const QString& annotationDir)
{
    QDir dir(annotationDir);
    if (!dir.exists()) {
        return QStringLiteral("Složka s anotacemi neexistuje: %1\n").arg(annotationDir);
    }

    dir.setNameFilters({QStringLiteral("page_*.txt")});
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList annotFiles = dir.entryInfoList();

    if (annotFiles.isEmpty()) {
        return QStringLiteral("Ve složce s anotacemi nebyly nalezeny žádné soubory.\n");
    }

    // classId → set of 1-based page numbers (sorted/unique)
    // classId → number of detections (can be > number of pages: same class
    // detected multiple times on the same page).
    QMap<int, QList<int>> pagesByClass;
    QMap<int, int>        countByClass;

    static const QRegularExpression pageRe(QStringLiteral("^page_(\\d+)$"));

    for (const QFileInfo& fi : annotFiles) {
        const auto match = pageRe.match(fi.completeBaseName());
        if (!match.hasMatch())
            continue;
        const int pageNum = match.captured(1).toInt();   // 1-based, as written by Python

        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QSet<int> classesOnThisPage;
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty())
                continue;

            const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (parts.isEmpty())
                continue;

            bool ok = false;
            const int classId = parts.first().toInt(&ok);
            if (!ok)
                continue;

            countByClass[classId] += 1;
            classesOnThisPage.insert(classId);
        }
        for (int cls : classesOnThisPage)
            pagesByClass[cls].append(pageNum);
    }

    for (auto it = pagesByClass.begin(); it != pagesByClass.end(); ++it) {
        std::sort(it.value().begin(), it.value().end());
    }

    struct SegRow {
        QString name;
        int     count;
        QString pages;
        QString missing;
    };

    std::vector<SegRow> rows;
    QSet<int> knownClasses;
    const auto& names = BB_NAMES();

    // --- PŘÍPRAVA POMOCNÝCH PROMĚNNÝCH PRO LOGIKU ---
    // 1. Zjistíme, jestli existuje alespoň jedna z tříd 0, 1, 3, 4
    //    (kontrolujeme, jestli pro ně máme nějaké stránky, ne jen jestli jsou v names)
    bool hasAnyMainClass = !pagesByClass.value(0).isEmpty() ||
                           !pagesByClass.value(1).isEmpty() ||
                           !pagesByClass.value(3).isEmpty() ||
                           !pagesByClass.value(4).isEmpty();

    // 2. Zjistíme počet stránek pro třídu 0 a přítomnost třídy 5
    int  class0PageCount =  pagesByClass.value(0).size();
    bool hasClass5       = !pagesByClass.value(5).isEmpty();

    // -----------------------------------------------

    for (auto it = names.begin(); it != names.end(); ++it) {
        const int classId = it.key();
        knownClasses.insert(classId);

        const auto pagesForClass = pagesByClass.value(classId);
        const int  countForClass = countByClass.value(classId, 0);

        QStringList pageStrs;
        for (int p : pagesForClass)
            pageStrs << QString::number(p);

        // Formátování stránek
        QString pagesFormatted = pageStrs.isEmpty()
                                     ? QStringLiteral("—")
                                     : pageStrs.join(QStringLiteral(", "));

        // --- ZJIŠTĚNÍ, JESTLI TŘÍDA CHYBÍ ---
        // Třída chybí, pokud pro ni nemáme žádné stránky a žádný počet výskytů.
        const bool classIsMissing = pagesForClass.isEmpty() && countForClass == 0;

        QString missingStatus = "";
        if (classIsMissing && classId!=5) {
            missingStatus = QStringLiteral("CHYBÍ ❌");
        }

        if (class0PageCount >= 3 && !hasClass5  && classId==5) {
            missingStatus = QStringLiteral("CHYBÍ ❌");
        }

        rows.push_back({
            it.value().name,
            countForClass,
            pagesFormatted,
            missingStatus
        });
    }

    constexpr int w1 = 21;
    constexpr int w2 = 7;
    constexpr int w3 = 32;
    constexpr int w4 = 10;

    QString result;
    QTextStream out(&result);

    auto writeRow = [&](const QString& a, const QString& b, const QString& c, const QString& d) {
        out << "| " << fitToWidth(a, w1 - 1)
        << "| " << fitToWidth(b, w2 - 1)
        << "| " << fitToWidth(c, w3 - 1)
        << "| " << fitToWidth(d, w4 - 1)
        << "|\n";
    };

    auto writeSeparator = [&](QChar ch) {
        out << "|" << QString(10, ch)
        << "|" << QString(w2, ch)
        << "|" << QString(w3, ch)
        << "|" << QString(w4, ch)
        << "|\n";
    };

    writeSeparator('=');
    writeRow(QStringLiteral("Náležitost"), QStringLiteral("Počet"),
             QStringLiteral("Strany"), QStringLiteral("Chybí"));
    writeSeparator('=');

    int totalDetections = 0;
    for (const auto& r : rows) {
        writeRow(r.name, QString::number(r.count), r.pages, r.missing);
        totalDetections += r.count;
    }
    writeSeparator('=');

    out << "\n";
    out << "Zpracováno stran: " << annotFiles.size() << "\n";
    out << "Celkem detekcí: "  << totalDetections   << "\n";


    return result;
}
