#ifndef CONTROLZPMZ_H
#define CONTROLZPMZ_H

#include <QString>

class FilePDF;
class FileNonPDF;
class GeometricPlan;

/**
 * @class ControlZPMZ
 * @brief Controller for the ZPMZ document set.
 *
 * Does not own its file data — it operates on the matching slots of a
 * GeometricPlan, which is the single source of truth. The controller binds
 * to that plan at construction and resolves each FileType to the
 * corresponding slot on demand.
 *
 * It deals only with GeometricPlan, FilePDF and FileNonPDF: it resolves a
 * FileType to its slot and forwards plain path/clear operations to it. It
 * derives nothing from the file system (display name, format, …) — the
 * caller (MainWindow) populates a slot through the reference accessors and
 * owns all such "sorting"/presentation logic.
 */
class ControlZPMZ
{
public:
    enum FileType { Popispole, Nacrt, Zap, Prot, Vymery, Sezvlast, Oprav, Dsps, Vytyc, GNSS, Vfk };

    /**
     * @brief Bind the controller to @p gp.
     * @param gp GeometricPlan that owns the file data.
     */
    explicit ControlZPMZ(GeometricPlan& gp);

    /** @name Slot accessors
     *  Direct references to the bound GeometricPlan slots, so the caller can
     *  populate them itself (deriving display name, format, present-state, …).
     *  pdf() resolves the ten PDF document slots; vfk() resolves the single
     *  FileNonPDF slot. pdf() must not be called with @c Vfk — use vfk().
     *  @{ */
    FilePDF& pdf(FileType type);
    const FilePDF& pdf(FileType type) const;
    FileNonPDF& vfk();
    const FileNonPDF& vfk() const;
    /** @} */

    QString getFilePath(FileType type) const;
    void clearFile(FileType type);

private:
    GeometricPlan* m_gp;  ///< The owning GeometricPlan whose slots this controls.
};

#endif // CONTROLZPMZ_H
