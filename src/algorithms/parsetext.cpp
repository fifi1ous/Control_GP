#include "parsetext.h"

#include <algorithm>

#include <QDebug>

namespace {

// Integer-digit width of a valid S-JTSK coordinate (positive/"reduced" KN
// convention). Y always has exactly 6 integer digits; X has 6–7. OCR on thin
// plotter fonts frequently duplicates a coordinate's leading digit, making the
// integer part one (or more) digits too long ("5568385.78" for "568385.78").
constexpr int kYIntDigits    = 6;   // Y: 6 integer digits
constexpr int kXMinIntDigits = 6;   // X: 6–7 integer digits
constexpr int kXMaxIntDigits = 7;

// Decide whether an OCR token has the shape of an S-JTSK coordinate and hand
// back its FULL value for storage/display.
//
// The token must read as a number with exactly two decimal places, and its
// integer part must be between `minIntDigits` and `maxIntDigits` wide. When OCR
// has duplicated leading digits the integer part is too long; we peel the extra
// leading digit(s) off for the width comparison only (zyyyyyy.yy -> yyyyyy.yy),
// but `out` always receives every digit the token had — the value is never
// trimmed, so the displayed coordinate is the unmodified OCR number.
//
// Returns true and writes the full value to `out` when the token matches the
// coordinate shape; false otherwise.
bool matchCoordinate(const QString& token, int minIntDigits, int maxIntDigits, double& out)
{
    static const QRegularExpression re(QStringLiteral("^[+-]?\\d+\\.\\d{2}$"));
    if (!re.match(token).hasMatch())
        return false;

    bool ok = false;
    out = token.toDouble(&ok);        // store/display the full, untrimmed value
    if (!ok)
        return false;

    // Integer digits only, sign stripped.
    const int dot = token.indexOf('.');
    QString intPart = token.left(dot);
    if (intPart.startsWith('-') || intPart.startsWith('+'))
        intPart.remove(0, 1);

    // Peel OCR-duplicated leading digits off so the width check sees the
    // canonical coordinate width — comparison only, `out` keeps the full value.
    while (intPart.size() > maxIntDigits)
        intPart.remove(0, 1);

    return intPart.size() >= minIntDigits && intPart.size() <= maxIntDigits;
}

} // namespace

ParseText::ParseText() {}

std::vector<Coordinates> ParseText::parseSSTextOCR(const QString& numeric, const QString& alphaNumeric)
{
    std::vector<Coordinates> output_coordinates;

    QStringList lines_an = alphaNumeric.split("\n");
    QStringList lines_a  = numeric.split("\n");

    // The numeric and alphanumeric OCR passes run independently and can yield
    // a different number of lines, so iterate only over the lines both share —
    // indexing past the shorter list would read out of bounds and crash.
    //const int line_count = std::min(lines_a.size(), lines_an.size());

    for (int i = 0; i < lines_a.size(); i++)
    {
        Coordinates coordinate;
        QString trimmed_an = lines_an[i].trimmed();
        QString trimmed_n  = lines_a[i].trimmed();

        QStringList parts_an = trimmed_an.split(QRegularExpression("\\s{2,}"));
        QStringList parts_n = trimmed_n.split(QRegularExpression("\\s+"));

        if (parts_n.isEmpty())
        {
            continue;
        }

        if (parts_n.size() >= 3)
        {
            qDebug() << trimmed_n;
            bool isFirstPartInt = false;
            int firstValue = parts_n.first().toInt(&isFirstPartInt);

            // Fallback logic if the first item is not a simple integer
            if (!isFirstPartInt) {
                QStringList dashParts = parts_n.first().split('-');

                if (dashParts.size() == 2) {
                    bool isPart1Int = false;
                    bool isPart2Int = false;

                    int val1 = dashParts[0].toInt(&isPart1Int);
                    int val2 = dashParts[1].toInt(&isPart2Int);

                    // If both parts are valid integers, generate the composite ID
                    if (isPart1Int && isPart2Int) {
                        firstValue = (val1 * 10000) + val2;
                        isFirstPartInt = true;
                    }
                }
            }

            if (!isFirstPartInt) {
                // OCR couldn't read the point id — keep the point anyway with
                // id 0 and match its coordinates by their format below.
                firstValue = 0;
            }

            double y1 = 0.0;
            double x1 = 0.0;
            if (!matchCoordinate(parts_n[1], kYIntDigits, kYIntDigits, y1) ||
                !matchCoordinate(parts_n[2], kXMinIntDigits, kXMaxIntDigits, x1)) {
                // Y/X don't have the shape of an S-JTSK coordinate — OCR garbage.
                // Warn so the point isn't lost silently and skip.
                qWarning() << "parseSSTextOCR: bod" << firstValue
                           << "má neplatné souřadnice (Y=" << parts_n[1]
                           << "X=" << parts_n[2] << ") — vynechán, "
                           << "OCR může vyžadovat manuální kontrolu";
                continue;
            }

            coordinate.id = firstValue;
            coordinate.Y1  = y1;
            coordinate.X1  = x1;
            coordinate.kv2 = parts_n.size() > 3 ? parts_n[3].toShort() : 0;

            if (parts_n.size() == 6)
            {
                double y2 = 0.0;
                double x2 = 0.0;
                if (matchCoordinate(parts_n[4], kYIntDigits, kYIntDigits, y2) &&
                    matchCoordinate(parts_n[5], kXMinIntDigits, kXMaxIntDigits, x2))
                {
                    coordinate.Y2  = y2;
                    coordinate.X2  = x2;
                }
            }
            coordinate.description = parts_an.last();

            coordinate.ocr = true;
            output_coordinates.push_back(coordinate);
        }
    }

    return output_coordinates;
}

std::vector<Coordinates> ParseText::parseSSTextPDF(const QString& text)
{
    std::vector<Coordinates> output_coordinates;
    QStringList lines = text.split("\n");

    for (const auto& line : lines)
    {
        QString trimmed = line.trimmed();

        // Skip completely empty lines right away
        if (trimmed.isEmpty())
        {
            continue;
        }

        // Use Qt::SkipEmptyParts to prevent lists with empty string elements
        QStringList parts_space_2 = trimmed.split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
        QStringList parts         = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (parts.isEmpty())
        {
            continue;
        }

        bool isIdValid = false;
        // Try to parse the ID normally first
        int id = parts[0].toInt(&isIdValid);

        // Fallback logic: If it's not a normal int, check if it's a dashed format (e.g., "707-22")
        if (!isIdValid && parts[0].contains('-')) {
            QStringList dashParts = parts[0].split('-');

            if (dashParts.size() == 2) {
                bool isPart1Int = false;
                bool isPart2Int = false;

                int val1 = dashParts[0].toInt(&isPart1Int);
                int val2 = dashParts[1].toInt(&isPart2Int);

                // If both sides of the dash are valid integers, generate the 7-digit composite ID
                if (isPart1Int && isPart2Int) {
                    id = (val1 * 10000) + val2;
                    isIdValid = true;
                }
            }
        }

        // If the first part is a valid number (either natively or via fallback), we have a coordinate line
        if (isIdValid)
        {
            Coordinates coordinate;
            coordinate.id = id;

            // Make sure we actually have at least 4 items before pulling them
            if (parts.size() >= 4)
            {
                coordinate.Y1  = parts[1].toDouble();
                coordinate.X1  = parts[2].toDouble();
                coordinate.kv2 = parts[3].toShort();
            }

            // Fix the crash: Require size >= 6 to safely access parts[4] AND parts[5]
            if (parts.size() >= 6)
            {
                bool y2Ok = false;
                bool x2Ok = false;

                double tempY2 = parts[4].toDouble(&y2Ok);
                double tempX2 = parts[5].toDouble(&x2Ok);

                if (y2Ok && x2Ok)
                {
                    coordinate.Y2 = tempY2;
                    coordinate.X2 = tempX2;
                }
            }

            if (!parts_space_2.isEmpty())
            {
                coordinate.description = parts_space_2.last();
            }
            // Only add the coordinate if it was successfully parsed
            output_coordinates.push_back(coordinate);
        }
    }

    return output_coordinates;
}

Coordinates ParseText::parseSSTextProt(const QString& text)
{
    Coordinates coordinate;

    QString trimmed = text.trimmed();
    trimmed.replace('|', ' ');

    const QStringList parts =
        trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    if (parts.size() < 3)        // not even id + Y1 + X1
        return coordinate;       // (optionally set an error/ocr flag here)

    auto isCoord = [](const QString& s) {
        bool ok = false;
        s.toDouble(&ok);
        return ok && s.contains('.');   // coordinates always have a decimal point
    };
    auto isKv = [](const QString& s) {
        static const QRegularExpression re("^\\d+$");
        return re.match(s).hasMatch();  // pure integer
    };

    int i = 0;

    // always-present leading fields
    coordinate.cb_full = parts[i++].toInt();
    coordinate.Y1      = parts[i++].toDouble();
    coordinate.X1      = parts[i++].toDouble();

    // optional kv1 (small integer before the second coordinate pair)
    if (i < parts.size() && isKv(parts[i]))
        coordinate.kv1 = parts[i++].toShort();

    // optional second coordinate pair
    if (i < parts.size() && isCoord(parts[i])) {
        coordinate.Y2 = parts[i++].toDouble();
        if (i < parts.size() && isCoord(parts[i]))
            coordinate.X2 = parts[i++].toDouble();
    }

    // optional kv2
    if (i < parts.size() && isKv(parts[i]))
        coordinate.kv2 = parts[i++].toShort();

    // whatever remains is free-text description
    if (i < parts.size())
        coordinate.description = parts.mid(i).join(' ');

    return coordinate;
}

std::vector<Coordinates> ParseText::parseSSTextProtPasted(const QString& text)
{
    std::vector<Coordinates> ss;

    QStringList lines  = text.split("\n");


    for (const auto& line : lines)
    {
        ss.push_back(parseSSTextProt(line));
    }
    return ss;
}

namespace {

// ── OCR "Výkaz dosavadního a nového stavu" parser ──────────────────────────
//
// The text-layer path (process_vymery.py -> ProcessGP.extract_vykaz) recovers
// the table columns from the precise character geometry pypdf's layout mode
// produces. For image-only (scanned) výkazy there is no text layer, so the
// page is rasterised and OCR'd (OcrManager, PSM 6 + preserve_interword_spaces).
// OCR keeps each line's *internal* spacing but rescales lines independently, so
// header-anchored column geometry is unreliable across rows. This parser instead
// reconstructs cells from the repeating data-row structure:
//
//     [dos_id  area  druh] | [nov_id  area  druh  zpus] | [porovnání …]
//
// The table's vertical rules survive OCR as '|', so segments are split on a
// run of ≥2 spaces or a '|'. A segment begins a new dosavadní/nový cell when it
// starts with a parcel id and the previous segment ended in a druh word. Every
// accepted stav must carry a real druh pozemku (validDruh), which keeps the
// porovnání block — parcel ids followed by a list-of-ownership number, never a
// druh — from being mistaken for a nový stav.

const QRegularExpression kIdRe(QStringLiteral(R"(^(?:st\.?\s*)?\d+(?:/\d+)?$)"));
const QRegularExpression kSegSep(QStringLiteral(R"(\s{2,}|\s*\|\s*)"));
const QRegularExpression kDigits(QStringLiteral(R"(\d+)"));
const QRegularExpression kWs(QStringLiteral(R"(\s+)"));
const QRegularExpression kNonDigit(QStringLiteral(R"([^0-9])"));
// Footnote marker (e.g. "*1)"): only treated as a note when an asterisk is
// present — a bare "1)" is almost always OCR border noise on an area digit.
const QRegularExpression kNoteRe(QStringLiteral(R"(^(?:\*\d+\)?\*?|\*?\d+\)\*)$)"));

// Roots of the Czech druh pozemku names (orná půda, zahrada, zastavěná plocha,
// ostatní plocha, …). A parsed stav is only accepted when its type text matches
// one of these, which rejects porovnání rows whose "type" reads "celá".
const char* const kDruhRoots[] = {
    "orn", "zahrad", "zahr", "zast", "ostat", "trav", "les", "vodn",
    "plocha", "ploch", "dům", "dum", "komun", "ovoc", "chmel",
    "vinic", "neplod", "dráh", "půda", "puda",
};

bool hasAlpha(const QString& s)
{
    for (const QChar c : s)
        if (c.isLetter())
            return true;
    return false;
}

QString normId(QString tok)
{
    tok.remove(' ');
    while (!tok.isEmpty() && QStringLiteral(".|);,").contains(tok.back()))
        tok.chop(1);
    return tok;
}

bool isParcelId(const QString& tok)
{
    const QString n = normId(tok);
    if (!kIdRe.match(n).hasMatch())
        return false;
    // Reject a bare single digit ("1".."9"): in noisy OCR these are stray marks,
    // not parcels — a real lone-digit parcel is vanishingly rare.
    return !(n.size() == 1 && n[0].isDigit());
}

bool isStrongId(const QString& tok)
{
    const QString n = normId(tok);
    return isParcelId(tok) && (n.contains('/') || n.startsWith(QStringLiteral("st")));
}

bool validDruh(const QString& d)
{
    const QString dl = d.toLower();
    for (const char* root : kDruhRoots)
        if (dl.contains(QString::fromUtf8(root)))
            return true;
    return false;
}

// Fold the ha / a / m² figures into a single area in m², mirroring
// ProcessGP._vykaz_area: 3 groups -> ha*10000 + a*100 + m², 2 -> a*100 + m².
int foldArea(const QStringList& nums)
{
    if (nums.size() >= 3)
        return nums[0].toInt() * 10000 + nums[1].toInt() * 100 + nums[2].toInt();
    if (nums.size() == 2)
        return nums[0].toInt() * 100 + nums[1].toInt();
    if (nums.size() == 1)
        return nums[0].toInt();
    return 0;
}

QStringList tokens(const QString& s)
{
    return s.split(kWs, Qt::SkipEmptyParts);
}

QString firstTok(const QString& seg)
{
    const QStringList t = tokens(seg);
    return t.isEmpty() ? QString() : t.first();
}

// The segment's last token is a druh word (text, not an id) — i.e. the segment
// ends a dosavadní/nový cell, so the next parcel id starts a new cell.
bool endsWithDruh(const QString& seg)
{
    const QStringList t = tokens(seg);
    if (t.isEmpty())
        return false;
    const QString last = t.last();
    return hasAlpha(last) && !isParcelId(last);
}

struct Stav {
    bool    ok = false;
    QString id;
    int     area = 0;
    QString druh;
};

// Parse one "id  výměra…  druh pozemku" cell. The id is the first token, the
// following run of numeric tokens (OCR separators stripped) is the výměra, and
// the trailing text tokens are the druh.
Stav parseStav(const QString& cell)
{
    QStringList toks;
    for (const QString& t : tokens(cell))
        if (!kNoteRe.match(t).hasMatch())
            toks << t;

    Stav s;
    if (toks.isEmpty() || !isParcelId(toks.first()))
        return s;

    s.id = normId(toks.first());
    int i = 1;
    QStringList nums;
    for (; i < toks.size(); ++i) {
        if (hasAlpha(toks[i]))          // druh starts here
            break;
        QString d = toks[i];
        d.remove(kNonDigit);            // drop OCR separators ; | ) ,
        if (!d.isEmpty())
            nums << d;                  // pure-punctuation tokens are skipped
    }
    QStringList druhWords;
    for (; i < toks.size(); ++i)
        if (hasAlpha(toks[i]))
            druhWords << toks[i];

    s.area = foldArea(nums);
    s.druh = druhWords.join(' ');
    s.ok = true;
    return s;
}

// Map a parsed parcel id ("315/2", "st.36/1") and its fields onto a Vymery,
// matching the field layout the text-layer path fills in PythonManager.
Vymery makeVymery(short stav, const QString& id, int area,
                  const QString& druh, short zpus)
{
    Vymery v;
    v.stav_dat = stav;
    v.cislovani = QString();
    v.kmenove = 0;
    v.poddeleni = 0;
    v.vymera = area;
    v.zpus_urc = zpus;
    v.druh_poz = druh;
    v.OCR = true;

    QString p = id;
    if (p.startsWith(QStringLiteral("st."))) {
        v.cislovani = QStringLiteral("st.");
        p.remove(0, 3);
    } else if (p.startsWith(QStringLiteral("st"))) {
        v.cislovani = QStringLiteral("st.");
        p.remove(0, 2);
    }
    const QStringList parts = p.split('/');
    if (!parts.isEmpty())
        v.kmenove = parts[0].toInt();
    if (parts.size() > 1)
        v.poddeleni = parts[1].toInt();
    return v;
}

// A header / caption line: drop lines carrying two or more table-header words
// so they are never parsed as data.
bool isHeaderOrBlank(const QString& ln)
{
    static const char* const kHeaderWords[] = {
        "Označení", "Výměra", "pozemku", "parcely", "parc",
        "Druh", "Způsob", "Způs", "využití", "Typ",
        "Porovnání", "přechází", "Číslo",
        "listu", "vlastnictví", "VÝKAZ", "DOSAVAD", "stav",
        "určení", "Oprávněný", "břemen",
    };
    if (ln.trimmed().isEmpty())
        return true;
    int hits = 0;
    for (const char* w : kHeaderWords)
        if (ln.contains(QString::fromUtf8(w)))
            ++hits;
    return hits >= 2;
}

} // namespace

void ParseText::parseVymeryTextOcr(const QString& numeric, const QString& alphanumeric,
                                   std::vector<Vymery>& vymeryOld,
                                   std::vector<Vymery>& vymeryNew,
                                   std::vector<Vymery>& vymeryPor)
{
    // The numeric-only OCR pass is too noisy for this table (the whitelist turns
    // header text into digit soup); the alphanumeric pass keeps the data rows
    // legible and is what we parse. The argument is kept for signature parity
    // with the SS path and as a hook for future cross-validation.
    Q_UNUSED(numeric);

    bool seen = false;
    const QStringList lines = alphanumeric.split('\n');

    for (const QString& rawLine : lines) {
        if (isHeaderOrBlank(rawLine))
            continue;

        const QStringList segs = rawLine.trimmed().split(kSegSep, Qt::SkipEmptyParts);
        if (segs.isEmpty())
            continue;

        // Součtový (totals) řádek: no real parcel id and two segments folding to
        // the same nonzero value (dosavadní total == nový total).
        bool anyStrong = false;
        for (const QString& s : segs)
            if (isStrongId(firstTok(s))) { anyStrong = true; break; }
        if (!anyStrong) {
            QList<int> groups;
            for (const QString& s : segs) {
                if (!s.contains(kDigits))
                    continue;
                QStringList g;
                auto it = kDigits.globalMatch(s);
                while (it.hasNext())
                    g << it.next().captured(0);
                const int v = foldArea(g);
                if (v)
                    groups << v;
            }
            if (seen && groups.size() >= 2 && groups[0] == groups[1] && groups[0] >= 10) {
                Vymery v = makeVymery(3, QString(), 0, QString(), 0);
                v.vymera = groups[0] - groups[1];   // mirrors the text-layer path
                vymeryPor.push_back(v);
                continue;
            }
        }

        // Cell-start ids: a parcel id at column 0, or one whose previous segment
        // ended in a druh word.
        QList<int> starts;
        for (int i = 0; i < segs.size(); ++i)
            if (isParcelId(firstTok(segs[i])) && (i == 0 || endsWithDruh(segs[i - 1])))
                starts << i;
        if (starts.isEmpty())
            continue;

        const bool hasDos = starts.first() == 0;
        int novStart = -1;
        for (int i : starts)
            if (i != 0) { novStart = i; break; }

        Stav d;
        if (hasDos) {
            const int dosEnd = (novStart >= 0) ? novStart : segs.size();
            d = parseStav(QStringList(segs.mid(0, dosEnd)).join(' '));
        }

        Stav n;
        short nz = 0;
        if (novStart >= 0) {
            // nový cell runs to the next cell-start id (the porovnání block) or
            // the end of the row.
            int novEnd = segs.size();
            for (int i : starts)
                if (i > novStart) { novEnd = i; break; }
            const QString novCell = QStringList(segs.mid(novStart, novEnd - novStart)).join(' ');
            n = parseStav(novCell);
            for (const QString& t : tokens(novCell))
                if (t == QStringLiteral("0") || t == QStringLiteral("1") || t == QStringLiteral("2"))
                    nz = static_cast<short>(t.toShort());
        } else if (!hasDos) {
            // A single id outside the dosavadní column -> treat it as a nový entry.
            n = parseStav(QStringList(segs.mid(starts.first())).join(' '));
        }

        const bool dOk = d.ok && d.area && validDruh(d.druh);
        const bool nOk = n.ok && n.area && validDruh(n.druh);
        if (dOk || nOk) {
            seen = true;
            if (dOk)
                vymeryOld.push_back(makeVymery(1, d.id, d.area, d.druh, 0));
            if (nOk)
                vymeryNew.push_back(makeVymery(2, n.id, n.area, n.druh, nz));
        }
    }
}