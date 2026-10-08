/**
 * @file geometricplan.h
 * @brief Declaration of the GeometricPlan class — aggregate of all files in a cadastral geometric plan.
 */

#ifndef GEOMETRICPLAN_H
#define GEOMETRICPLAN_H

#include <vector>

#include "filepdf.h"
#include "filenonpdf.h"
#include "fileother.h"
#include <stdexcept>


/**
 * @class GeometricPlan
 * @brief Container for every file that makes up a single geometric plan submission.
 *
 * Aggregates the standard PDF documents (popispole, nacrt, zap, prot,
 * vymery, sezvlast, oprav, dsps, vytyc, gp, zadost, overeni), the
 * standard non-PDF files (vfk, ss, AZI1-3) and any additional files the
 * author included via a vector of FileOther entries. Provides typed
 * setter/getter pairs for each named slot and an indexed API for the
 * "other" vector with bounds checking.
 */
class GeometricPlan
{
private:
    FilePDF popispole;  ///< Popisové pole — description field PDF.
    FilePDF nacrt;      ///< Náčrt — sketch PDF.
    FilePDF zap;        ///< Záznam podrobného měření změn (ZPMZ) PDF.
    FilePDF prot;       ///< Protocol PDF.
    FilePDF vymery;     ///< Výkaz výměr — area statement PDF.
    FilePDF sezvlast;   ///< Seznam vlastníků — list of owners PDF.
    FilePDF oprav;      ///< Correction PDF.
    FilePDF dsps;       ///< DSPS PDF.
    FilePDF vytyc;      ///< Vytyčovací náčrt — staking sketch PDF.
    FilePDF gnss;       ///< GNSS measurement protocol PDF.
    FilePDF gp;         ///< Geometric plan PDF.
    FilePDF zadost;     ///< Žádost — application/request PDF.
    FilePDF overeni;    ///< Ověření — verification PDF.

    FileNonPDF vfk;     ///< VFK exchange-format file.
    FileNonPDF ss;      ///<
    FileNonPDF AZI1;    ///< AZI measurement file 1.
    FileNonPDF AZI2;    ///< AZI measurement file 2.
    FileNonPDF AZI3;    ///< AZI measurement file 3.

    std::vector<FileOther> other;   ///< Additional files included by the author beyond the standard set.

    /**
     * @brief Validate that @p index is within @ref other bounds.
     * @param index        Zero-based index into the other-files vector.
     * @param functionName Caller name, embedded in the exception message.
     * @throws std::out_of_range If @p index is negative or not less than @c other.size().
     */
    void validateIndex(int index, const char* functionName) const
    {
        if (index < 0 || index >= static_cast<int>(other.size()))
        {
            throw std::out_of_range(QString("%1: Index out of range").arg(functionName).toStdString());
        }
    }

public:
    /**
     * @brief Default constructor. Default-initializes every file slot and leaves @ref other empty.
     */
    GeometricPlan();

    /** @name PDF setters
     *  @{ */
    void setPopispole(const FilePDF& popispole);    ///< Set the popispole PDF entry.
    void setNacrt(const FilePDF& nacrt);            ///< Set the nacrt PDF entry.
    void setZap(const FilePDF& zap);                ///< Set the zap PDF entry.
    void setProt(const FilePDF& prot);              ///< Set the prot PDF entry.
    void setVymery(const FilePDF& vymery);          ///< Set the vymery PDF entry.
    void setSezvlast(const FilePDF& sezvlast);      ///< Set the sezvlast PDF entry.
    void setOprav(const FilePDF& oprav);            ///< Set the oprav PDF entry.
    void setDsps(const FilePDF& dsps);              ///< Set the dsps PDF entry.
    void setVytyc(const FilePDF& vytyc);            ///< Set the vytyc PDF entry.
    void setGnss(const FilePDF& gnss);              ///< Set the GNSS PDF entry.
    void setGp(const FilePDF& gp);                  ///< Set the gp PDF entry.
    void setOvereni(const FilePDF& overeni);        ///< Set the overeni PDF entry.
    void setZadost(const FilePDF& zadost);          ///< Set the zadost PDF entry.
    /** @} */

    /** @name Non-PDF setters
     *  @{ */
    void setVfk(const FileNonPDF& vfk);             ///< Set the VFK file entry.
    void setSs(const FileNonPDF& ss);               ///< Set the SS file entry.
    void setAZI1(const FileNonPDF& AZI1);           ///< Set the AZI1 file entry.
    void setAZI2(const FileNonPDF& AZI2);           ///< Set the AZI2 file entry.
    void setAZI3(const FileNonPDF& AZI3);           ///< Set the AZI3 file entry.
    /** @} */

    /** @name PDF getters
     *  @{ */
    const FilePDF& getPopispole() const;    ///< Get the popispole PDF entry.
    const FilePDF& getNacrt() const;        ///< Get the nacrt PDF entry.
    const FilePDF& getZap() const;          ///< Get the zap PDF entry.
    const FilePDF& getProt() const;         ///< Get the prot PDF entry.
    const FilePDF& getVymery() const;       ///< Get the vymery PDF entry.
    const FilePDF& getSezvlast() const;     ///< Get the sezvlast PDF entry.
    const FilePDF& getOprav() const;        ///< Get the oprav PDF entry.
    const FilePDF& getDsps() const;         ///< Get the dsps PDF entry.
    const FilePDF& getVytyc() const;        ///< Get the vytyc PDF entry.
    const FilePDF& getGnss() const;         ///< Get the GNSS PDF entry.
    const FilePDF& getGp() const;           ///< Get the gp PDF entry.
    const FilePDF& getOvereni() const;      ///< Get the overeni PDF entry.
    const FilePDF& getZadost() const;       ///< Get the zadost PDF entry.
    /** @} */

    /** @name Non-PDF getters
     *  @{ */
    const FileNonPDF& getVfk() const;       ///< Get the VFK file entry.
    const FileNonPDF& getSs() const;        ///< Get the SS file entry.
    const FileNonPDF& getAZI1() const;      ///< Get the AZI1 file entry.
    const FileNonPDF& getAZI2() const;      ///< Get the AZI2 file entry.
    const FileNonPDF& getAZI3() const;      ///< Get the AZI3 file entry.
    /** @} */

    /** @name Mutable slot accessors
     *  Direct references to the underlying file slots, used by the
     *  controllers (ControlGeometricPlan, ControlZadost, ControlZPMZ) so
     *  that the GeometricPlan remains the single source of truth and the
     *  controllers operate on its parts rather than holding their own copies.
     *  @{ */
    FilePDF& refPopispole();    ///< Mutable reference to the popispole slot.
    FilePDF& refNacrt();        ///< Mutable reference to the nacrt slot.
    FilePDF& refZap();          ///< Mutable reference to the zap slot.
    FilePDF& refProt();         ///< Mutable reference to the prot slot.
    FilePDF& refVymery();       ///< Mutable reference to the vymery slot.
    FilePDF& refSezvlast();     ///< Mutable reference to the sezvlast slot.
    FilePDF& refOprav();        ///< Mutable reference to the oprav slot.
    FilePDF& refDsps();         ///< Mutable reference to the dsps slot.
    FilePDF& refVytyc();        ///< Mutable reference to the vytyc slot.
    FilePDF& refGnss();         ///< Mutable reference to the gnss slot.
    FilePDF& refGp();           ///< Mutable reference to the gp slot.
    FilePDF& refZadost();       ///< Mutable reference to the zadost slot.
    FileNonPDF& refVfk();       ///< Mutable reference to the vfk slot.
    /** @} */

    /** @name Other-files API
     *  Operations on the variable-length vector of additional files.
     *  @{ */

    /**
     * @brief Replace the entire collection of other files.
     * @param otherFiles New vector of FileOther entries.
     */
    void setOther(const std::vector<FileOther>& otherFiles);

    /**
     * @brief Append a single other-file entry.
     * @param otherFile Entry to append to the back of the vector.
     */
    void pushOther(const FileOther& otherFile);

    /**
     * @brief Remove every entry from the other-files vector.
     */
    void clearOther();

    /**
     * @brief Remove every occurrence of @p otherFile from the vector.
     * @param otherFile Entry to remove; equality is determined by FileOther::operator==.
     */
    void removeOther(const FileOther& otherFile);

    /**
     * @brief Read-only access to the other-files vector.
     * @return Const reference to the underlying vector.
     */
    const std::vector<FileOther>& getOther() const;

    /**
     * @brief Number of entries currently stored in the other-files vector.
     * @return Size as an int (cast from std::size_t).
     */
    int getOtherSize() const;

    /**
     * @brief Access an other-file entry by index.
     * @param index Zero-based index into the vector.
     * @return Const reference to the requested entry.
     * @throws std::out_of_range If @p index is invalid.
     */
    const FileOther& getOtherAt(int index) const;

    /**
     * @brief Remove an other-file entry by index.
     * @param index Zero-based index into the vector.
     * @throws std::out_of_range If @p index is invalid.
     */
    void removeOtherAt(int index);
    /** @} */

    /** @name File removal
     *  Reset a named file slot back to its empty/default state (via the
     *  file's clearAll()), or clear every file in the plan at once.
     *  @{ */

    /** @name PDF removers
     *  @{ */
    void removePopispole();     ///< Reset the popispole PDF slot to empty.
    void removeNacrt();         ///< Reset the nacrt PDF slot to empty.
    void removeZap();           ///< Reset the zap PDF slot to empty.
    void removeProt();          ///< Reset the prot PDF slot to empty.
    void removeVymery();        ///< Reset the vymery PDF slot to empty.
    void removeSezvlast();      ///< Reset the sezvlast PDF slot to empty.
    void removeOprav();         ///< Reset the oprav PDF slot to empty.
    void removeDsps();          ///< Reset the dsps PDF slot to empty.
    void removeVytyc();         ///< Reset the vytyc PDF slot to empty.
    void removeGnss();          ///< Reset the GNSS PDF slot to empty.
    void removeGp();            ///< Reset the gp PDF slot to empty.
    void removeZadost();        ///< Reset the zadost PDF slot to empty.
    void removeOvereni();       ///< Reset the overeni PDF slot to empty.
    /** @} */

    /** @name Non-PDF removers
     *  @{ */
    void removeVfk();           ///< Reset the VFK file slot to empty.
    void removeSs();            ///< Reset the SS file slot to empty.
    void removeAZI1();          ///< Reset the AZI1 file slot to empty.
    void removeAZI2();          ///< Reset the AZI2 file slot to empty.
    void removeAZI3();          ///< Reset the AZI3 file slot to empty.
    /** @} */

    /**
     * @brief Remove every file in the plan at once.
     *
     * Resets all named PDF and non-PDF slots to their empty/default state
     * and clears the other-files vector.
     */
    void removeAll();
    /** @} */
};

#endif // GEOMETRICPLAN_H
