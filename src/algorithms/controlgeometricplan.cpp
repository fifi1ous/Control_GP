#include "controlgeometricplan.h"

#include "geometricplan.h"
#include "filepdf.h"

#include <QMap>
#include <QHash>
#include <QStringList>
#include <cmath>
#include <cstdlib>
#include <functional>

namespace {

/// Copy the parsed coordinate fields of one source into a comparison slot.
void fillValues(SSPointValues& v, const Coordinates& c)
{
    v.present     = true;
    v.Y1          = c.Y1;
    v.X1          = c.X1;
    v.kv1         = c.kv1;
    v.Y2          = c.Y2;
    v.X2          = c.X2;
    v.kv2         = c.kv2;
    v.description = c.description;
    v.ocr         = c.ocr;
}

} // namespace

ControlGeometricPlan::ControlGeometricPlan(GeometricPlan& gp)
    : m_file(&gp.refGp()) {}

QString ControlGeometricPlan::getPath() const { return m_file->getPath(); }

SSControlReport ControlGeometricPlan::controlSS(const std::vector<Coordinates>& ssGp,
                                                const std::vector<Coordinates>& ssVfk,
                                                const std::vector<Coordinates>& ssProt,
                                                double tolerance) const
{
    SSControlReport report;
    report.tolerance = tolerance;

    // Linking scheme:
    //   náčrt (ssGp) ──id──▶ VFK (ssVfk) ──cb_full──▶ protokol (ssProt)
    //
    // The VFK is the bridge: it carries both the short point number (id == cb,
    // shared with the náčrt) and the full point number (cb_full, shared with
    // the protocol). Rows are therefore keyed by the short number (id); the
    // náčrt joins on id, the protocol joins on cb_full and is translated back
    // to id via the VFK. The náčrt and the VFK are the controlled pair — the
    // protocol is only a helper, so points that appear in the protocol alone
    // are reported but not treated as errors. QMap keeps the rows ordered.
    QMap<int, SSPointComparison> byId;    // id (short cb) -> merged row
    QHash<int, int> fullToId;             // cb_full -> id, learned from the VFK

    // VFK first: it defines the id ↔ cb_full bridge used by the other sources.
    for (const Coordinates& c : ssVfk) {
        if (c.id == 0) continue;            // skip parse failures (no point number)
        if (c.cb_full != 0) fullToId.insert(c.cb_full, c.id);
        SSPointComparison& row = byId[c.id];
        row.id = c.id;
        if (c.cb_full != 0) row.cbFull = c.cb_full;
        fillValues(row.vfk, c);
    }

    // Radius for the coordinate-based fallback match below. When a náčrt point
    // cannot be linked to a VFK row by point number — the id is missing (OCR
    // dropped it) or was misread into a number the VFK does not know — we fall
    // back to the row whose first coordinate pair lies within this distance on
    // BOTH axes. 14 cm absorbs rounding / OCR digit noise while staying far
    // below the spacing between distinct survey points.
    constexpr double kCoordMatchRadius = 0.14;   // metres (= 14 cm), per axis

    // Find the free VFK row nearest @p c within kCoordMatchRadius on each axis.
    // "Free" = the VFK listed the point but no náčrt point has claimed it yet,
    // so two náčrt points can never collapse onto one VFK row. Returns the id
    // key of the best match, or 0 when none is close enough.
    auto matchVfkByCoords = [&](const Coordinates& c) -> int {
        int    bestKey  = 0;
        double bestDist = 0.0;
        for (auto it = byId.begin(); it != byId.end(); ++it) {
            const SSPointComparison& r = it.value();
            if (!r.vfk.present || r.gp.present) continue;   // need a free VFK row
            const double dY = std::abs(c.Y1 - r.vfk.Y1);
            const double dX = std::abs(c.X1 - r.vfk.X1);
            if (dY > kCoordMatchRadius || dX > kCoordMatchRadius) continue;
            const double dist = dY * dY + dX * dX;          // nearest one wins
            if (bestKey == 0 || dist < bestDist) { bestKey = it.key(); bestDist = dist; }
        }
        return bestKey;
    };

    // Náčrt: joins the VFK on the short point number (id). When the id is not
    // a short number the VFK knows, fall back to treating it as a full point
    // number (cb_full) and translating it back to the short number through the
    // VFK bridge — this rescues náčrt points the VFK records under cb_full only.
    // When neither id route links the point, fall back to matching it onto a
    // VFK row by coordinates (±14 cm), so OCR'd points with a missing or wrong
    // point number are still cross-checked instead of being dropped.
    for (const Coordinates& c : ssGp) {
        if (c.ocr) report.gpOcr = true;   // náčrt coords came from OCR somewhere

        // 1. Link by the short point number (id), or via the cb_full bridge.
        int  key    = 0;
        bool linked = false;
        if (c.id != 0) {
            auto vfkRow = byId.constFind(c.id);
            if (vfkRow != byId.constEnd() && vfkRow->vfk.present) {
                key = c.id;
                linked = true;
            } else {
                auto bridged = fullToId.constFind(c.id);   // try gp.id as a cb_full
                if (bridged != fullToId.constEnd()) { key = bridged.value(); linked = true; }
            }
        }

        // 2. No id link: match by coordinates onto a free VFK row.
        if (!linked) {
            const int coordKey = matchVfkByCoords(c);
            if (coordKey != 0) { key = coordKey; linked = true; }
        }

        // 3. Still unlinked: keep an id-keyed row so the point surfaces as
        //    "missing in VFK". With neither an id nor a coordinate match there
        //    is no row to place it on, so it is dropped (as before).
        if (!linked) {
            if (c.id == 0) continue;
            key = c.id;
        }

        SSPointComparison& row = byId[key];
        if (row.id == 0) row.id = key;
        fillValues(row.gp, c);
    }

    // Protocol: joins the VFK on the full point number (cb_full). Translate it
    // to the short number via the bridge so it lands on the matching row.
    // Points the VFK does not know are collected as protocol-only orphans.
    QMap<int, SSPointComparison> protOnly;   // cb_full -> protocol-only row
    for (const Coordinates& c : ssProt) {
        if (c.cb_full == 0) continue;
        auto it = fullToId.constFind(c.cb_full);
        if (it == fullToId.constEnd()) {
            SSPointComparison& row = protOnly[c.cb_full];
            row.cbFull = c.cb_full;
            fillValues(row.prot, c);
            continue;
        }
        SSPointComparison& row = byId[it.value()];
        if (row.cbFull == 0) row.cbFull = c.cb_full;
        fillValues(row.prot, c);
    }

    auto close = [tolerance](double a, double b) {
        return std::abs(a - b) <= tolerance;
    };

    // Across the sources that are present and carry a given field, every value
    // must agree. @p has selects the sources that carry the field (always true
    // for the mandatory fields), @p get reads it. Empty / single-source fields
    // trivially agree — there is nothing to disagree with.
    auto agree = [&](const SSPointComparison& row,
                     std::function<bool(const SSPointValues&)> has,
                     std::function<double(const SSPointValues&)> get) {
        const SSPointValues* sources[] = { &row.gp, &row.vfk, &row.prot };
        bool haveRef = false;
        double ref = 0.0;
        for (const SSPointValues* s : sources) {
            if (!s->present || !has(*s)) continue;
            if (!haveRef) { ref = get(*s); haveRef = true; }
            else if (!close(ref, get(*s))) return false;
        }
        return true;
    };

    auto always = [](const SSPointValues&) { return true; };

    auto evaluate = [&](SSPointComparison& row) {
        // Náčrt and VFK are the controlled pair; either one missing is an error.
        QStringList missing;
        if (!row.gp.present)  missing << QStringLiteral("GP");
        if (!row.vfk.present) missing << QStringLiteral("VFK");
        if (!missing.isEmpty()) {
            row.status = CheckStatus::Error;
            row.note   = QStringLiteral("Chybí v: ") + missing.join(QStringLiteral(", "));
            ++report.incomplete;
            return;
        }

        // Both present: compare the mandatory fields, plus the optional ones
        // across whichever sources actually carry them. tolerance is 0 by
        // default, i.e. the coordinates must match exactly.
        QStringList diffs;
        if (!agree(row, always, [](const SSPointValues& s) { return s.Y1; }))
            diffs << QStringLiteral("Y1");
        if (!agree(row, always, [](const SSPointValues& s) { return s.X1; }))
            diffs << QStringLiteral("X1");
        // kv1 is optional: not every source carries a first-pair quality code
        // (e.g. the náčrt records only the position quality, stored as kv2).
        if (!agree(row, [](const SSPointValues& s) { return s.hasKv1(); },
                        [](const SSPointValues& s) { return double(s.kv1); }))
            diffs << QStringLiteral("kv1");

        if (!agree(row, [](const SSPointValues& s) { return s.hasPair2(); },
                        [](const SSPointValues& s) { return s.Y2; }))
            diffs << QStringLiteral("Y2");
        if (!agree(row, [](const SSPointValues& s) { return s.hasPair2(); },
                        [](const SSPointValues& s) { return s.X2; }))
            diffs << QStringLiteral("X2");
        if (!agree(row, [](const SSPointValues& s) { return s.hasKv2(); },
                        [](const SSPointValues& s) { return double(s.kv2); }))
            diffs << QStringLiteral("kv2");

        // Description (free text): exact match across the sources that carry it.
        {
            bool haveRef = false;
            QString ref;
            for (const SSPointValues* s : { &row.gp, &row.vfk, &row.prot }) {
                if (!s->present || !s->hasDesc()) continue;
                if (!haveRef) { ref = s->description; haveRef = true; }
                else if (ref != s->description) { diffs << QStringLiteral("popis"); break; }
            }
        }

        if (diffs.isEmpty()) {
            row.status = CheckStatus::Ok;
            row.note   = QStringLiteral("Souhlasí");
            ++report.matched;
        } else {
            row.status = CheckStatus::Error;
            row.note   = QStringLiteral("Liší se: ") + diffs.join(QStringLiteral(", "));
            ++report.mismatched;
        }
    };

    for (auto it = byId.begin(); it != byId.end(); ++it) {
        SSPointComparison row = it.value();
        evaluate(row);
        report.points.push_back(row);
    }

    // Protocol-only points: reported for completeness but not an error.
    for (auto it = protOnly.begin(); it != protOnly.end(); ++it) {
        SSPointComparison row = it.value();
        row.status = CheckStatus::Info;
        row.note   = QStringLiteral("Pouze v protokolu");
        ++report.protOnly;
        report.points.push_back(row);
    }

    return report;
}

namespace {

/// Copy the parsed fields of one Vymery into a comparison slot.
void fillVymery(VymeryValues& v, const Vymery& s)
{
    v.present   = true;
    v.cislovani = s.cislovani;
    v.kmenove   = s.kmenove;
    v.poddeleni = s.poddeleni;
    v.vymera    = s.vymera;
    v.vymera_d  = s.vymery_d;
    v.zpus_urc  = s.zpus_urc;
    v.druh_poz  = s.druh_poz;
    v.ocr       = s.OCR;
}

/// Stable key for a parcel: numbering type + base number + subdivision. QMap
/// keeps the rows ordered by this key so parcels print in a predictable order.
QString parcelKey(const Vymery& s)
{
    return s.cislovani + QStringLiteral("|")
         + QString::number(s.kmenove) + QStringLiteral("|")
         + QString::number(s.poddeleni);
}

} // namespace

VymeryControlReport ControlGeometricPlan::controlVymery(const VymeryInputs& in,
                                                        int tolerance) const
{
    VymeryControlReport report;
    report.tolerance = tolerance;

    // Merge the per-source vectors of one state (old or new) into one row per
    // parcel, keyed by the (cislovani, kmenove, poddeleni) triple. @p slot picks
    // which slot of the row a given source feeds.
    auto merge = [&report](QMap<QString, VymeryComparison>& rows,
                           const std::vector<Vymery>& src,
                           VymeryValues VymeryComparison::* slot,
                           bool withProt) {
        for (const Vymery& s : src) {
            VymeryComparison& row = rows[parcelKey(s)];
            row.cislovani = s.cislovani;
            row.kmenove   = s.kmenove;
            row.poddeleni = s.poddeleni;
            row.withProt  = withProt;
            fillVymery(row.*slot, s);
            if (s.OCR) report.ocr = true;
        }
    };

    // Are two carried areas equal within the per-parcel rounding tolerance? A
    // single square metre of rounding is allowed so the area determined by the
    // GP need not match the VFK to the last unit.
    auto vymeraClose = [](int a, int b) { return std::abs(a - b) <= 1; };

    // Evaluate one merged parcel row. The controlled sources are the GP, the VFK
    // and the Výměry; the protocol (new state only) is informative. The verdict
    // and the running tallies depend on which state we are in, so the caller
    // passes the matching counters.
    auto evaluate = [&](VymeryComparison& row,
                        int& matched, int& mismatched, int& incomplete) {
        const VymeryValues* controlled[] = { &row.gp, &row.vfk, &row.vymery };
        const QString names[] = { QStringLiteral("GP"),
                                  QStringLiteral("VFK"),
                                  QStringLiteral("Výměry") };

        QStringList missing;
        for (int i = 0; i < 3; ++i)
            if (!controlled[i]->present) missing << names[i];
        if (!missing.isEmpty()) {
            row.status = CheckStatus::Error;
            row.note   = QStringLiteral("Chybí v: ") + missing.join(QStringLiteral(", "));
            ++incomplete;
            return;
        }

        QStringList diffs;

        // Výměra: the main controlled value — every controlled source must agree.
        {
            bool haveRef = false; int ref = 0;
            for (const VymeryValues* s : controlled) {
                if (!haveRef) { ref = s->vymera; haveRef = true; }
                else if (!vymeraClose(ref, s->vymera)) { diffs << QStringLiteral("výměra"); break; }
            }
        }

        // Zpus_urc (method of area determination): must agree across the sources
        // that carry it (it is optional in some výkaz formats).
        {
            bool haveRef = false; short ref = 0;
            for (const VymeryValues* s : controlled) {
                if (!s->hasZpusUrc()) continue;
                if (!haveRef) { ref = s->zpus_urc; haveRef = true; }
                else if (ref != s->zpus_urc) { diffs << QStringLiteral("způsob určení"); break; }
            }
        }

        // Protocol (new state): informative only. A differing area is noted but
        // does not change the verdict.
        QString protNote;
        if (row.withProt && row.prot.present
            && !vymeraClose(row.vfk.vymera, row.prot.vymera))
            protNote = QStringLiteral("; protokol: výměra se liší (informativně)");

        if (diffs.isEmpty()) {
            row.status = CheckStatus::Ok;
            row.note   = QStringLiteral("Souhlasí") + protNote;
            ++matched;
        } else {
            row.status = CheckStatus::Error;
            row.note   = QStringLiteral("Liší se: ") + diffs.join(QStringLiteral(", ")) + protNote;
            ++mismatched;
        }
    };

    // --- Old (dosavadní) state: GP, VFK, Výměry ---
    QMap<QString, VymeryComparison> oldRows;
    merge(oldRows, in.oldGp,  &VymeryComparison::gp,     false);
    merge(oldRows, in.oldVfk, &VymeryComparison::vfk,    false);
    merge(oldRows, in.oldVym, &VymeryComparison::vymery, false);
    for (auto it = oldRows.begin(); it != oldRows.end(); ++it) {
        VymeryComparison row = it.value();
        evaluate(row, report.oldMatched, report.oldMismatched, report.oldIncomplete);
        report.oldParcels.push_back(row);
    }

    // --- New (nový) state: GP, VFK, Výměry plus the informative protocol ---
    QMap<QString, VymeryComparison> newRows;
    merge(newRows, in.newGp,   &VymeryComparison::gp,     true);
    merge(newRows, in.newVfk,  &VymeryComparison::vfk,    true);
    merge(newRows, in.newVym,  &VymeryComparison::vymery, true);
    merge(newRows, in.newProt, &VymeryComparison::prot,   true);
    for (auto it = newRows.begin(); it != newRows.end(); ++it) {
        VymeryComparison row = it.value();
        evaluate(row, report.newMatched, report.newMismatched, report.newIncomplete);
        report.newParcels.push_back(row);
    }

    // --- Balance: total old area against total new area ---------------------
    // Each parcel's representative area: prefer the VFK, then the GP, then the
    // Výměry, so a parcel still contributes when one source omitted it.
    auto repArea = [](const VymeryComparison& row) {
        if (row.vfk.present)    return row.vfk.vymera;
        if (row.gp.present)     return row.gp.vymera;
        if (row.vymery.present) return row.vymery.vymera;
        return 0;
    };
    for (const VymeryComparison& row : report.oldParcels) report.sumOld += repArea(row);
    for (const VymeryComparison& row : report.newParcels) report.sumNew += repArea(row);

    // The por (porovnání) column is the per-parcel rounding correction: every
    // entry must be −1, 0 or +1, otherwise the old and new states do not balance.
    int porCount = 0;
    for (const std::vector<Vymery>* por : { &in.porGp, &in.porVym })
        for (const Vymery& p : *por) {
            ++porCount;
            if (std::abs(p.vymera) > 1) ++report.porBad;
        }

    // Each parcel may legitimately round by at most ±1 m², so unless the caller
    // overrides it the allowed difference between the totals is the number of
    // por corrections. The verdict holds when no correction is out of range and
    // the totals stay within that tolerance.
    report.tolerance = (tolerance > 0) ? tolerance : porCount;
    report.balanceOk = (report.porBad == 0)
                       && (std::abs(report.sumNew - report.sumOld) <= report.tolerance);

    return report;
}
