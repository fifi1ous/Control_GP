#include "controlpdf.h"

#include "filepdf.h"
#include "geometricplan.h"

#include <QSet>
#include <QStringList>

#include "maps.h"

namespace {

// Cadastral conformance thresholds.
constexpr float kMinPdfVersion  = 1.7f;   ///< Minimum acceptable PDF version.
constexpr float kMinPdfaVersion = 2.0f;   ///< Minimum acceptable PDF/A version.

} // namespace

ControlPDF::ControlPDF(const GeometricPlan& plan)
    : m_plan(&plan) {}

PdfControlReportFormat ControlPDF::controlFile(const FilePDF& file,
                                         const QString& categoryKey,
                                         int expectedSignatures,
                                         bool largeFormatAllowed) const
{
    PdfControlReportFormat report;
    report.fileName = file.getName();
    report.present  = true;     // only present files reach this point

    // Predicted content label (the NAMESMAP "second string"); fall back to the
    // raw predicted token when it is not a known category.
    const QString predicted = file.getWhatItIs();
    report.category = NAMESMAP.value(predicted, predicted);

    // --- File name must contain the predicted content token ---
    // The match is case-insensitive ("Náčrt" / "nacrt" both pass). Žádost is
    // exempt: its file name does not carry the category token.
    if (categoryKey.compare(QStringLiteral("zadost"), Qt::CaseInsensitive) != 0) {
        const bool nameOk = !predicted.isEmpty()
                            && file.getName().contains(predicted, Qt::CaseInsensitive);
        report.checks.push_back({
            QStringLiteral("Název obsahuje typ"),
            predicted.isEmpty() ? QStringLiteral("—") : predicted,
            nameOk ? CheckStatus::Ok : CheckStatus::Error,
            nameOk ? QString()
                   : QStringLiteral("Název souboru musí obsahovat „%1“").arg(predicted)
        });
    }

    // --- PDF version (≥ 1.7) ---
    const float pdfVersion = file.getPDFVersion();
    const bool  pdfVersionOk = pdfVersion >= kMinPdfVersion;
    report.checks.push_back({
        QStringLiteral("Verze PDF"),
        QString::number(pdfVersion, 'f', 1),
        pdfVersionOk ? CheckStatus::Ok : CheckStatus::Error,
        pdfVersionOk ? QString() : QStringLiteral("Vyžadováno ≥ 1.7")
    });

    // --- PDF/A (must be PDF/A with version ≥ 2; show the version text) ---
    const bool  isPdfa     = file.getPDFA();
    const float pdfaVersion = file.getPDFAVersion();
    const bool  pdfaOk     = isPdfa && pdfaVersion >= kMinPdfaVersion;
    const QString pdfaValue = !isPdfa
                                  ? QStringLiteral("Ne")
                                  : (file.getVersion().isEmpty()
                                         ? QString::number(pdfaVersion, 'f', 0)
                                         : file.getVersion());
    report.checks.push_back({
        QStringLiteral("PDF/A"),
        pdfaValue,
        pdfaOk ? CheckStatus::Ok : CheckStatus::Error,
        pdfaOk ? QString()
               : (isPdfa ? QStringLiteral("Vyžadováno PDF/A ≥ 2")
                         : QStringLiteral("Soubor musí být PDF/A"))
    });

    // --- Number of signatures (1 for GP/Žádost, 0 otherwise) ---
    const int  signatures = file.getNumSignatures();
    const bool sigOk      = signatures == expectedSignatures;
    report.checks.push_back({
        QStringLiteral("Počet podpisů"),
        QString::number(signatures),
        sigOk ? CheckStatus::Ok : CheckStatus::Error,
        sigOk ? QString() : QStringLiteral("Očekáváno %1").arg(expectedSignatures)
    });

    // --- Can add signature (must be allowed) ---
    const bool canSign = file.getCanSignature();
    report.checks.push_back({
        QStringLiteral("Lze podepsat"),
        canSign ? QStringLiteral("Ano") : QStringLiteral("Ne"),
        canSign ? CheckStatus::Ok : CheckStatus::Error,
        canSign ? QString() : QStringLiteral("Dokument neumožňuje přidání podpisu")
    });

    // --- Number of pages (informational) ---
    report.checks.push_back({
        QStringLiteral("Počet stran"),
        QString::number(file.getNumPages()),
        CheckStatus::Info,
        QString()
    });

    // --- Page formats (deduplicated via a set; A4 only, or A1–A4 for GP/Náčrt) ---
    static const QSet<QString> kAllowedLarge = {
        QStringLiteral("A1"), QStringLiteral("A2"),
        QStringLiteral("A3"), QStringLiteral("A4")
    };
    static const QSet<QString> kAllowedSmall = { QStringLiteral("A4") };
    const QSet<QString>& allowed = largeFormatAllowed ? kAllowedLarge : kAllowedSmall;

    QSet<QString> uniqueFormats;
    for (const QString& fmt : file.getFormats()) {
        QString norm = fmt;
        if (norm.isEmpty())
            norm = QStringLiteral("?");     // unrecognized page size
        uniqueFormats.insert(norm);
    }

    bool formatsOk = !uniqueFormats.isEmpty();
    for (const QString& fmt : uniqueFormats) {
        if (!allowed.contains(fmt)) {
            formatsOk = false;
            break;
        }
    }

    QStringList formatList(uniqueFormats.begin(), uniqueFormats.end());
    formatList.sort();

    report.checks.push_back({
        QStringLiteral("Formáty stran"),
        formatList.isEmpty() ? QStringLiteral("—") : formatList.join(QStringLiteral(", ")),
        formatsOk ? CheckStatus::Ok : CheckStatus::Error,
        formatsOk ? QString()
                  : (largeFormatAllowed ? QStringLiteral("Povoleno pouze A1–A4")
                                        : QStringLiteral("Povoleno pouze A4"))
    });

    return report;
}

std::vector<PdfControlReportFormat> ControlPDF::control_PDF() const
{
    const GeometricPlan& gp = *m_plan;

    // Each standard PDF slot, its NAMESMAP key, the required signature count
    // and whether large (A1–A4) page formats are permitted. Ověření is
    // deliberately omitted — it is not subject to these checks.
    struct Slot {
        const FilePDF* file;
        QString        key;
        int            expectedSignatures;
        bool           largeFormat;
    };


    const std::vector<Slot> slots = {
        { &gp.getGp(),        QStringLiteral("GP"),        1, true  },
        { &gp.getZadost(),    QStringLiteral("zadost"),    1, false },
        { &gp.getNacrt(),     QStringLiteral("nacrt"),     0, true  },
        { &gp.getPopispole(), QStringLiteral("popispole"), 0, false },
        { &gp.getZap(),       QStringLiteral("zap"),       0, false },
        { &gp.getProt(),      QStringLiteral("prot"),      0, false },
        { &gp.getVymery(),    QStringLiteral("vymery"),    0, false },
        { &gp.getSezvlast(),  QStringLiteral("sezvlast"),  0, false },
        { &gp.getOprav(),     QStringLiteral("oprav"),     0, false },
        { &gp.getDsps(),      QStringLiteral("dsps"),      0, false },
        { &gp.getVytyc(),     QStringLiteral("vytyc"),     0, false },
    };

    std::vector<PdfControlReportFormat> reports;
    reports.reserve(slots.size());
    for (const Slot& s : slots) {
        if (!s.file->getPresent())
            continue;
        reports.push_back(controlFile(*s.file, s.key, s.expectedSignatures, s.largeFormat));
    }
    return reports;
}

PdfControlReportFormat ControlPDF::control_single_PDF(FilePDF const &file)
{
    const QString what_it_is = file.getWhatItIs();

    // Per-category control parameters — required signature count and whether
    // large (A1–A4) page formats are permitted. Mirrors the slot table in
    // control_PDF(). Anything unknown falls back to the strict default
    // (no signature, A4 only).
    struct Params { int expectedSignatures; bool largeFormat; };
    static const QMap<QString, Params> kParams = {
        { QStringLiteral("gp"),        { 1, true  } },
        { QStringLiteral("zadost"),    { 1, false } },
        { QStringLiteral("nacrt"),     { 0, true  } },
        { QStringLiteral("popispole"), { 0, false } },
        { QStringLiteral("zap"),       { 0, false } },
        { QStringLiteral("prot"),      { 0, false } },
        { QStringLiteral("vymery"),    { 0, false } },
        { QStringLiteral("sezvlast"),  { 0, false } },
        { QStringLiteral("oprav"),     { 0, false } },
        { QStringLiteral("dsps"),      { 0, false } },
        { QStringLiteral("vytyc"),     { 0, false } },
    };

    const Params params = kParams.value(what_it_is.toLower(), { 0, false });
    return controlFile(file, what_it_is, params.expectedSignatures, params.largeFormat);
}

NonPdfControlReport ControlPDF::control_nonPDF() const
{
    const GeometricPlan& gp = *m_plan;
    NonPdfControlReport report;

    // --- VFK (name and format if present) ---
    const FileNonPDF& vfk = gp.getVfk();
    report.vfkPresent = vfk.getPresent();
    if (report.vfkPresent) {
        report.vfkName   = vfk.getName();
        report.vfkFormat = vfk.getFormat();
    }

    const FileNonPDF& ss = gp.getSs();
    report.ssPresent = ss.getPresent();
    if (report.ssPresent) {
        report.ssName   = ss.getName();
        report.ssFormat = ss.getFormat();
    }

    // --- Signatures: AZI1 signed files, AZI2 signature, AZI3 time stamp ---
    struct SigSlot { const FileNonPDF* file; QString label; };
    const std::vector<SigSlot> sigSlots = {
        { &gp.getAZI1(), QStringLiteral("Podepsané soubory (AZI1)") },
        { &gp.getAZI2(), QStringLiteral("Podpis (AZI2)") },
        { &gp.getAZI3(), QStringLiteral("Časové razítko (AZI3)") },
    };
    for (const SigSlot& s : sigSlots) {
        SignatureEntry e;
        e.label   = s.label;
        e.present = s.file->getPresent();
        if (e.present) {
            e.name   = s.file->getName();
            e.format = s.file->getFormat();
        }
        report.signatures.push_back(e);
    }

    // --- GNSS measurement ---
    report.gnssUsed = gp.getGnss().getPresent();

    // --- Signature verification (Ověření) ---
    const FilePDF& overeni = gp.getOvereni();
    report.overeniPresent = overeni.getPresent();
    if (report.overeniPresent)
        report.overeniName = overeni.getName();

    // --- Other (non-standard) files; PDFs also carry their predicted class ---
    for (const FileOther& o : gp.getOther()) {
        OtherFileEntry e;
        e.name   = o.getName();
        e.format = o.getFormat();
        e.isPdf  = o.getFormat().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
        if (e.isPdf)
            e.documentClass = NAMESMAP.value(o.getWhatItIs(), o.getWhatItIs());
        report.others.push_back(e);
    }

    return report;
}
