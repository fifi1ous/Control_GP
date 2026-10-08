#ifndef CONTROLGEOMETRICPLAN_H
#define CONTROLGEOMETRICPLAN_H

#include <QString>
#include <vector>

#include "structures.h"

class FilePDF;
class GeometricPlan;

/**
 * @class ControlGeometricPlan
 * @brief Controller for the geometric-plan PDF.
 *
 * Does not own its file data — it operates on the @c gp slot of a
 * GeometricPlan, which is the single source of truth. The controller
 * binds to that slot at construction and forwards all operations to it.
 */
class ControlGeometricPlan
{
public:
    /**
     * @brief Bind the controller to the @c gp slot of @p plan.
     * @param plan GeometricPlan that owns the file data.
     */
    explicit ControlGeometricPlan(GeometricPlan& gp);

    QString getPath() const;               ///< Get the path from the bound gp slot.

    /**
     * @brief Cross-check the SS coordinates obtained from the three sources.
     *
     * The náčrt and the VFK share the short point number (Coordinates::id ==
     * cb); the VFK and the protocol share the full point number (cb_full). The
     * VFK therefore bridges the two numbering schemes: rows are keyed by the
     * short number, the náčrt joining on @c id and the protocol joining on
     * @c cb_full (translated back to @c id through the VFK).
     *
     * The náčrt and the VFK are the controlled pair: a point present in both
     * with agreeing values is matched, a point missing from either is flagged
     * as incomplete. The protocol is only a helper — where it lists a point its
     * values must agree too, but a point appearing in the protocol alone is
     * reported as informative, not an error. The compared fields are Y1, X1 and
     * kv1, plus Y2, X2, kv2 and the description for the sources that carry them;
     * values must agree to within @p tolerance metres (0 = exact match).
     *
     * A náčrt point whose point number cannot be linked to the VFK (the id is
     * missing or was misread — typical of OCR'd náčrty) is matched onto the VFK
     * row whose first coordinate pair lies within 14 cm on both axes, so it is
     * still cross-checked rather than dropped. A point with neither a usable id
     * nor a coordinate match within that radius is ignored.
     *
     * Performs no formatting — pass the result to
     * DisplayResults::generateSSReport() to turn it into printable text.
     *
     * @param ssGp      Coordinates extracted from the geometric-plan drawing.
     * @param ssVfk     Coordinates parsed from the VFK file.
     * @param ssProt    Coordinates obtained from the protocol.
     * @param tolerance Maximum allowed coordinate difference, in metres.
     * @return A single SSControlReport describing every distinct point number.
     */
    SSControlReport controlSS(const std::vector<Coordinates>& ssGp,
                              const std::vector<Coordinates>& ssVfk,
                              const std::vector<Coordinates>& ssProt,
                              double tolerance = 0.0) const;

    /**
     * @brief Cross-check the area statement (výkaz výměr) across the sources.
     *
     * A parcel is identified by the triple (cislovani, kmenove, poddeleni). For
     * the old (dosavadní) state the controlled sources are the geometric plan,
     * the VFK and the Výměry; their @c vymera must agree and their @c zpus_urc
     * must agree wherever it is carried, while @c druh_poz is only a description
     * and is shown but not judged. The new (nový) state is checked the same way
     * and additionally lists the protocol, which carries only the parcel number
     * and the new area and is therefore informative — a difference there is
     * noted but is not an error.
     *
     * The por (porovnání) column holds the rounding correction of every parcel;
     * each entry must be −1, 0 or +1, otherwise the old and new states do not
     * balance. The report also totals the old and new areas so the caller can
     * see whether the whole statement balances within tolerance.
     *
     * Performs no formatting — pass the result to
     * DisplayResults::generateVymeryReport() to turn it into printable text.
     *
     * @param in        All per-source area-statement vectors.
     * @param tolerance Maximum allowed |sumNew − sumOld| difference, in m².
     * @return A single VymeryControlReport describing every distinct parcel.
     */
    VymeryControlReport controlVymery(const VymeryInputs& in,
                                      int tolerance = 0) const;

private:
    FilePDF* m_file;    ///< Points to the gp slot inside the owning GeometricPlan.
};

#endif // CONTROLGEOMETRICPLAN_H
