/**
 * @file structures.h
 * @brief Core data structures used throughout the Control_GP application.
 *
 * Defines VFK cadastral data structures (VfkSObr, VfkSPol, VfkPar)
 * and the BBox structure for PDF annotation bounding boxes.
 */

#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <QString>
#include <QRectF>

/**
 * @struct VfkSObr
 * @brief Represents a survey point (Souradnice Obrazu) from VFK data.
 *
 * Stores coordinates and metadata for a single survey point
 * as parsed from the SOBR block of a VFK file.
 */
struct VfkSObr
{
    int id;             ///< Unique identifier of the survey point.
    short stav_dat;     ///< Data state indicator.
    int ku;             ///< Cadastral unit code (katastralni uzemi).
    int zpmz;           ///< ZPMZ number (zpusob mereni a zobrazeni).
    int tl;             ///< Point type (typ linie).
    int cb;             ///< Point number (cislo bodu).
    int cb_uplne;       ///< Full point number.
    double y;           ///< Y coordinate (S-JTSK coordinate system).
    double x;           ///< X coordinate (S-JTSK coordinate system).
    short kk;           ///< Quality code (kod kvality).
};

/**
 * @struct VfkSPol
 * @brief Represents a polygon point (Souradnice Polygonu) from VFK data.
 *
 * Extends the survey point concept with additional measurement reference
 * fields. Parsed from the SPOL block of a VFK file.
 */
struct VfkSPol
{
    int id;                 ///< Unique identifier of the polygon point.
    short stav_dat;         ///< Data state indicator.
    int ku;                 ///< Cadastral unit code (katastralni uzemi).
    int zpmz;               ///< ZPMZ number (zpusob mereni a zobrazeni).
    int tl;                 ///< Point type (typ linie).
    int cb;                 ///< Point number (cislo bodu).
    int cb_uplne;           ///< Full point number.
    double y = 0.0;         ///< Y coordinate (S-JTSK coordinate system).
    double x = 0.0;               ///< X coordinate (S-JTSK coordinate system).
    short kk = 0;               ///< Quality code (kod kvality).
    int ku_kod_mereni;      ///< Cadastral unit code of measurement origin.
    int zpmz_cislo_mereni;  ///< ZPMZ number of measurement origin.
};

struct Coordinates
{
    int id;
    int cb_full;
    double Y1 = 0;
    double X1 = 0;
    short kv1 = 0;
    double Y2 = 0;
    double X2 = 0;
    short kv2 = 0;
    QString description;
    bool ocr = false;
};

/**
 * @struct VfkPar
 * @brief Represents a parcel record (Parcela) from VFK data.
 *
 * Contains comprehensive parcel information including identification,
 * temporal validity, geometric properties, and relationships to other
 * cadastral entities. Parsed from the PAR block of a VFK file.
 */
struct  VfkPar
{
    int id;                     ///< Unique identifier of the parcel.
    short stav_dat;             ///< Data state indicator.
    QString dat_vzniku;         ///< Date of creation.
    QString dat_zaniku;         ///< Date of deletion/expiration.
    short priznak_kontextu;     ///< Context flag.
    int rizeni_id_vzniku;       ///< ID of the proceeding that created this record.
    int rizeni_id_zaniku;       ///< ID of the proceeding that deleted this record.
    int pkn_id;                 ///< PKN identifier.
    int par_type;               ///< Parcel type.
    int kat_uz_kod;             ///< Cadastral unit code.
    int kat_uz_kod_puv;         ///< Original cadastral unit code.
    short druh_cislovani;       ///< Numbering type.
    int kmenove_cislo_par;      ///< Base parcel number.
    short zdpace_kod;           ///< Source code.
    short podeleni_cisla_par;   ///< Parcel number subdivision.
    short dil_parcely;          ///< Parcel part.
    int maplis_kod;             ///< Map sheet code.
    short zp_ur_vym;            ///< Method of area determination.
    short druh_poz;             ///< Land type.
    int zpus_vyuz;              ///< Land use type.
    short typ_parcely;          ///< Parcel type classification.
    int vymera;                 ///< Area in square meters.
    QString def_bod;            ///< Defining point.
    int tel_id;                 ///< TEL identifier.
    int par_id;                 ///< Parent parcel identifier.
    int bud_id;                 ///< Building identifier.
    QString ident_bud;          ///< Building identification string.
    QString soucasti;           ///< Components description.
    int ps_id;                  ///< PS identifier.
    QString ident_ps;           ///< PS identification string.
};

struct Vymery{
    short stav_dat;
    QString cislovani;
    int kmenove;
    int poddeleni;
    int vymera;
    double vymery_d = 0.0;
    short zpus_urc;
    QString druh_poz;
    bool OCR = false;
};

/**
 * @struct BBox
 * @brief Represents a bounding box annotation on a PDF page.
 *
 * Stores the class label and rectangular coordinates (in PDF-point space,
 * origin at top-left of the page) for a single detected object.
 */
struct BBox
{
    int     classId  = 0;   ///< Numeric class identifier for the detected object.
    QString className;      ///< Optional human-readable class label; falls back to "cls <id>".
    double  xMin = 0;       ///< Left edge of the bounding box in PDF points.
    double  yMin = 0;       ///< Top edge of the bounding box in PDF points.
    double  xMax = 0;       ///< Right edge of the bounding box in PDF points.
    double  yMax = 0;       ///< Bottom edge of the bounding box in PDF points.
};

/**
 * @struct MapPartsGP
 * @brief Represents parts of GP
 *
 * Stores name, abbreviationn and path to a folder for each part of GP
 */
struct MapPartsGP
{
    QString name;           ///< Name of the GP part
    QString abbreviation;   ///< Abbreviation of the GP part
    QString path;           ///< Path to the segment folder
};

struct InputFile
{
    QString category;
    QString name;
    QString format;
    QString path;
    bool    has_metadata        = false;
    float   pdf_version         = 0.0f;
    bool    is_pdfa             = false;
    float   pdfa_version_number = -1.0f;
    QString pdfa_version_description;
    int     number_of_signatures = 0;
    bool    can_add_signature    = false;
    int     number_of_pages      = 0;
    std::vector<QString> pages_formats;
};

/**
 * @enum CheckStatus
 * @brief Outcome of a single control check, used to pick the status glyph
 *        when a report is rendered.
 */
enum class CheckStatus
{
    Ok,         ///< Requirement satisfied.
    Error,      ///< Requirement violated.
    Info        ///< Neutral value, no pass/fail judgement.
};

/**
 * @struct CheckItem
 * @brief One row of a control report — a labelled check with its found value.
 *
 * Produced by the control layer (e.g. ControlPDF) and consumed by
 * DisplayResults, which is responsible for all formatting. Keeping the data
 * and its presentation separate means the same report can later be rendered
 * to a different sink (HTML, log, …) without touching the control logic.
 */
struct CheckItem
{
    QString     label;                      ///< What was checked, e.g. "PDF/A".
    QString     value;                      ///< The value found, e.g. "Ano" / "3".
    CheckStatus status = CheckStatus::Info; ///< Pass/fail/neutral judgement.
    QString     note;                       ///< Optional extra explanation.
};

/**
 * @struct PdfControlReport
 * @brief Structured result of controlling a single PDF file.
 *
 * A plain data container with no formatting logic: ControlPDF::control()
 * fills it in, DisplayResults::generatePdfReport() turns it into text.
 */
struct PdfControlReportFormat
{
    QString                fileName;        ///< Name of the controlled file.
    QString                category;        ///< Human-readable document type, if known.
    bool                   present = false; ///< Whether the file is present at all.
    std::vector<CheckItem> checks;          ///< Individual checks performed.
};

/**
 * @struct SignatureEntry
 * @brief Presence and identity of one signature-related file (AZI1–AZI3).
 *
 * AZI1 holds the signed files, AZI2 the signature itself and AZI3 the
 * time stamp. The label carries the human-readable role.
 */
struct SignatureEntry
{
    QString label;            ///< Human-readable role, e.g. "Podepsané soubory (AZI1)".
    bool    present = false;  ///< Whether this signature file is present.
    QString name;             ///< File name (only meaningful when present).
    QString format;           ///< File format/extension (only meaningful when present).
};

/**
 * @struct OtherFileEntry
 * @brief One additional, non-standard file in the non-PDF summary.
 */
struct OtherFileEntry
{
    QString name;                ///< File name.
    QString format;              ///< File format/extension.
    bool    isPdf = false;       ///< Whether the file is a PDF.
    QString documentClass;       ///< Predicted document class label (PDF only).
};

/**
 * @struct NonPdfControlReport
 * @brief Structured summary of the non-PDF members of a GeometricPlan.
 *
 * Covers the VFK file, the signature files (AZI1–AZI3), the GNSS
 * measurement, the signature verification (Ověření) and any additional
 * files included by the author. Like PdfControlReportFormat it carries no
 * formatting logic: ControlPDF::control_nonPDF() fills it in and
 * DisplayResults::generateNonPdfReport() turns it into text.
 */
struct NonPdfControlReport
{
    bool    vfkPresent = false;   ///< Whether the VFK file is present.
    QString vfkName;              ///< VFK file name.
    QString vfkFormat;            ///< VFK file format.

    bool    ssPresent = false;   ///< Whether the VFK file is present.
    QString ssName;              ///< VFK file name.
    QString ssFormat;            ///< VFK file format.

    std::vector<SignatureEntry> signatures; ///< AZI1–AZI3 signature files.

    bool    gnssUsed = false;     ///< Whether a GNSS measurement is present.

    bool    overeniPresent = false; ///< Whether the verification (Ověření) file is present.
    QString overeniName;            ///< Verification file name.

    std::vector<OtherFileEntry> others; ///< Additional, non-standard files.
};

/**
 * @struct SSPointValues
 * @brief The coordinate fields one source carries for a single survey point.
 *
 * The optional fields (the second coordinate pair, its quality code and the
 * description) are not provided by every source; the @c has* helpers report
 * whether this source actually carries them, so the comparison can skip a
 * field for sources that simply do not have it.
 */
struct SSPointValues
{
    bool    present = false;     ///< This source listed the point at all.

    double  Y1  = 0, X1 = 0;     ///< First (definitive) coordinate pair.
    short   kv1 = 0;             ///< Quality code of the first pair.

    double  Y2  = 0, X2 = 0;     ///< Optional second coordinate pair.
    short   kv2 = 0;             ///< Optional quality code of the second pair.
    QString description;         ///< Optional free-text description.

    /// True when this source's values were recovered with OCR (Tesseract)
    /// instead of being read from a text block — they may contain errors.
    bool    ocr = false;

    /// In S-JTSK the coordinates are large positive numbers, so a zero pair
    /// means the source simply did not carry a second coordinate pair.
    bool hasPair2() const { return Y2 != 0.0 || X2 != 0.0; }
    bool hasKv1()   const { return kv1 != 0; }
    bool hasKv2()   const { return kv2 != 0; }
    bool hasDesc()  const { return !description.isEmpty(); }
};

/**
 * @struct SSPointComparison
 * @brief Coordinates of a single survey point (SS) as seen by each of the
 *        three sources, together with the verdict of comparing them.
 *
 * The three sources are the geometric-plan drawing (náčrt), the VFK file and
 * the protocol. The náčrt and the VFK share the short point number (@c id ==
 * cb); the VFK and the protocol share the full point number (@c cbFull ==
 * cb_full). The VFK therefore bridges the two numbering schemes, and rows are
 * keyed by the short number.
 */
struct SSPointComparison
{
    int  id     = 0;   ///< Short point number (cb): the náčrt ↔ VFK link key.
    int  cbFull = 0;   ///< Full point number (cb_full): the VFK ↔ protokol link key.

    SSPointValues gp;    ///< Values from the geometric-plan drawing (náčrt).
    SSPointValues vfk;   ///< Values from the VFK file.
    SSPointValues prot;  ///< Values from the protocol.

    CheckStatus status = CheckStatus::Info; ///< Pass/fail/neutral verdict.
    QString     note;                        ///< Human-readable explanation.
};

/**
 * @struct SSControlReport
 * @brief Result of cross-checking the SS coordinates from the three sources.
 *
 * A plain data container with no formatting logic:
 * ControlGeometricPlan::controlSS() fills it in,
 * DisplayResults::generateSSReport() turns it into printable text.
 */
struct SSControlReport
{
    std::vector<SSPointComparison> points; ///< One row per distinct point number.
    int    matched    = 0;   ///< Present in both náčrt and VFK with agreeing values.
    int    mismatched = 0;   ///< Present in both náčrt and VFK but with differing values.
    int    incomplete = 0;   ///< Missing from the náčrt or the VFK.
    int    protOnly   = 0;   ///< Present only in the protocol (informative, not an error).
    double tolerance  = 0;   ///< Coordinate tolerance (metres) used for the comparison.

    /// True when at least one geometric-plan (náčrt) point was recovered with
    /// OCR instead of a text block; DisplayResults shows a warning about it.
    bool   gpOcr      = false;
};

/**
 * @struct VymeryValues
 * @brief The area-statement (výkaz výměr) fields one source carries for a
 *        single parcel.
 *
 * A parcel is identified by the triple (cislovani, kmenove, poddeleni); the
 * remaining fields are the values to be cross-checked. @c vymera is the main
 * controlled quantity and @c zpus_urc (method of area determination) must agree
 * across the GP, the VFK and the Výměry; @c druh_poz is only a description.
 *
 * @c zpus_urc is optional — some výkaz formats and the protocol do not carry it,
 * in which case it stays 0; @c hasZpusUrc() reports whether the source actually
 * provides it so the comparison can skip the field for sources that do not.
 */
struct VymeryValues
{
    bool    present   = false;  ///< This source listed the parcel at all.
    QString cislovani;          ///< Numbering type: "st." (stavební) or "" (pozemková).
    int     kmenove   = 0;      ///< Base parcel number (kmenové číslo).
    int     poddeleni = 0;      ///< Parcel subdivision (poddělení), 0 when none.
    int     vymera    = 0;      ///< Area in square metres — the main controlled value.
    double  vymera_d  = 0.0;    ///< Raw fractional area (protocol carries decimals).
    short   zpus_urc  = 0;      ///< Method of area determination (optional, 0 = absent).
    QString druh_poz;           ///< Land type — informative description only.

    /// True when this source's values were recovered with OCR and may be wrong.
    bool    ocr       = false;

    bool hasZpusUrc() const { return zpus_urc != 0; }
    bool hasDruhPoz() const { return !druh_poz.isEmpty(); }
};

/**
 * @struct VymeryComparison
 * @brief One parcel as seen by each source, with the verdict of comparing them.
 *
 * For the old (dosavadní) state the controlled sources are the geometric plan
 * (GP), the VFK and the Výměry. For the new (nový) state the protocol is added
 * as a fourth, purely informative source that carries only the parcel number
 * and the new area. Rows are keyed by the (cislovani, kmenove, poddeleni)
 * triple, which is duplicated here for display.
 */
struct VymeryComparison
{
    QString cislovani;          ///< Numbering type of the parcel ("st." or "").
    int     kmenove   = 0;      ///< Base parcel number.
    int     poddeleni = 0;      ///< Parcel subdivision (0 when none).

    VymeryValues gp;            ///< Values from the geometric plan (výkaz výměr).
    VymeryValues vfk;           ///< Values from the VFK file.
    VymeryValues vymery;        ///< Values from the Výměry (výpočet výměr) document.
    VymeryValues prot;          ///< Values from the protocol — new state only, informative.

    bool        withProt = false; ///< True for new-state rows that may carry a prot slot.
    CheckStatus status   = CheckStatus::Info; ///< Pass/fail/neutral verdict.
    QString     note;             ///< Human-readable explanation.
};

/**
 * @struct VymeryInputs
 * @brief The raw area-statement vectors gathered from every source.
 *
 * Bundles the per-source vectors so ControlGeometricPlan::controlVymery() can
 * take a single argument instead of nine. @c por* hold the rounding-correction
 * column (porovnání se stavem evidence), whose every entry is expected to be
 * −1, 0 or +1.
 */
struct VymeryInputs
{
    std::vector<Vymery> oldGp, newGp, porGp;   ///< Extracted from the geometric plan.
    std::vector<Vymery> oldVfk, newVfk;        ///< Parsed from the VFK file.
    std::vector<Vymery> oldVym, newVym, porVym;///< Extracted from the Výměry document.
    std::vector<Vymery> newProt;               ///< Extracted from the protocol (new, informative).
};

/**
 * @struct VymeryControlReport
 * @brief Result of cross-checking the area statement across the sources.
 *
 * A plain data container with no formatting logic:
 * ControlGeometricPlan::controlVymery() fills it in,
 * DisplayResults::generateVymeryReport() turns it into printable text.
 */
struct VymeryControlReport
{
    std::vector<VymeryComparison> oldParcels; ///< One row per old-state parcel.
    std::vector<VymeryComparison> newParcels; ///< One row per new-state parcel.

    int oldMatched = 0, oldMismatched = 0, oldIncomplete = 0; ///< Old-state tallies.
    int newMatched = 0, newMismatched = 0, newIncomplete = 0; ///< New-state tallies.

    // Old/new balance ("porovnání se stavem evidence právních vztahů"):
    // the total old area against the total new area.
    int  sumOld    = 0;     ///< Sum of the old-state areas.
    int  sumNew    = 0;     ///< Sum of the new-state areas.
    int  porBad    = 0;     ///< Por entries outside {-1, 0, +1} — an old/new mismatch.
    int  tolerance = 0;     ///< Allowed |sumNew − sumOld| difference (m²).
    bool balanceOk = false; ///< Whether the old and new totals are within tolerance.

    bool ocr = false;       ///< True when any source value came from OCR.
};

#endif // STRUCTURES_H
