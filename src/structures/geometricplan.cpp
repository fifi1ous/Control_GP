/**
 * @file geometricplan.cpp
 * @brief Implementation of the GeometricPlan class.
 */

#include "geometricplan.h"
#include <algorithm>

/**
 * @brief Default-construct every named file slot; @ref other starts empty.
 */
GeometricPlan::GeometricPlan()
    : popispole(), nacrt(), zap(), prot(), vymery(), sezvlast(), oprav(), dsps(), vytyc(), gnss(), gp(), zadost(), overeni(),
      vfk(), ss(), AZI1(), AZI2(), AZI3() {}

// Setters
void GeometricPlan::setPopispole(const FilePDF& popispole) { this->popispole = popispole; } // Set popispole
void GeometricPlan::setNacrt(const FilePDF& nacrt) { this->nacrt = nacrt; }                 // Set nacrt
void GeometricPlan::setZap(const FilePDF& zap) { this->zap = zap; }                         // Set zap
void GeometricPlan::setProt(const FilePDF& prot) { this->prot = prot; }                     // Set prot
void GeometricPlan::setVymery(const FilePDF& vymery) { this->vymery = vymery; }             // Set vymery
void GeometricPlan::setSezvlast(const FilePDF& sezvlast) { this->sezvlast = sezvlast; }     // Set sezvlast
void GeometricPlan::setOprav(const FilePDF& oprav) { this->oprav = oprav; }                 // Set oprav
void GeometricPlan::setDsps(const FilePDF& dsps) { this->dsps = dsps; }                     // Set dsps
void GeometricPlan::setVytyc(const FilePDF& vytyc) { this->vytyc = vytyc; }                 // Set vytyc
void GeometricPlan::setGnss(const FilePDF& gnss) { this->gnss = gnss; }                     // Set gnss
void GeometricPlan::setGp(const FilePDF& gp) { this->gp = gp; }                             // Set gp
void GeometricPlan::setVfk(const FileNonPDF& vfk) { this->vfk = vfk; }                      // Set vfk
void GeometricPlan::setSs(const FileNonPDF& ss) { this->ss = ss; }                          // Set ss
void GeometricPlan::setAZI1(const FileNonPDF& AZI1) { this->AZI1 = AZI1; }                  // Set AZI1
void GeometricPlan::setAZI2(const FileNonPDF& AZI2) { this->AZI2 = AZI2; }                  // Set AZI2
void GeometricPlan::setAZI3(const FileNonPDF& AZI3) { this->AZI3 = AZI3; }                  // Set AZI3

void GeometricPlan::setZadost(const FilePDF& zadost) { this->zadost = zadost; }             // Set Zadost
void GeometricPlan::setOvereni(const FilePDF& overeni) { this->overeni = overeni; }         // Set Overeni

// Getters
const FilePDF& GeometricPlan::getPopispole() const { return popispole; }                    // Get popispole
const FilePDF& GeometricPlan::getNacrt() const { return nacrt; }                            // Get nacrt
const FilePDF& GeometricPlan::getZap() const { return zap; }                                // Get zap     
const FilePDF& GeometricPlan::getProt() const { return prot; }                              // Get prot
const FilePDF& GeometricPlan::getVymery() const { return vymery; }                          // Get vymery   
const FilePDF& GeometricPlan::getSezvlast() const { return sezvlast; }                      // Get sezvlast
const FilePDF& GeometricPlan::getOprav() const { return oprav; }                            // Get oprav
const FilePDF& GeometricPlan::getDsps() const { return dsps; }                              // Get dsps
const FilePDF& GeometricPlan::getVytyc() const { return vytyc; }                            // Get vytyc
const FilePDF& GeometricPlan::getGnss() const { return gnss; }                              // Get gnss
const FilePDF& GeometricPlan::getGp() const { return gp; }                                  // Get gp
const FileNonPDF& GeometricPlan::getVfk() const { return vfk; }                             // Get vfk
const FileNonPDF& GeometricPlan::getSs() const { return ss; }                               // Get ss   
const FileNonPDF& GeometricPlan::getAZI1() const { return AZI1; }                           // Get AZI1
const FileNonPDF& GeometricPlan::getAZI2() const { return AZI2; }                           // Get AZI2   
const FileNonPDF& GeometricPlan::getAZI3() const { return AZI3; }                           // Get AZI3

const FilePDF& GeometricPlan::getZadost() const { return zadost; }                          // Get Zadost
const FilePDF& GeometricPlan::getOvereni() const { return overeni; }                        // Get Overeni

// Mutable slot accessors — expose the underlying slots to the controllers.
FilePDF& GeometricPlan::refPopispole() { return popispole; }
FilePDF& GeometricPlan::refNacrt() { return nacrt; }
FilePDF& GeometricPlan::refZap() { return zap; }
FilePDF& GeometricPlan::refProt() { return prot; }
FilePDF& GeometricPlan::refVymery() { return vymery; }
FilePDF& GeometricPlan::refSezvlast() { return sezvlast; }
FilePDF& GeometricPlan::refOprav() { return oprav; }
FilePDF& GeometricPlan::refDsps() { return dsps; }
FilePDF& GeometricPlan::refVytyc() { return vytyc; }
FilePDF& GeometricPlan::refGnss() { return gnss; }
FilePDF& GeometricPlan::refGp() { return gp; }
FilePDF& GeometricPlan::refZadost() { return zadost; }
FileNonPDF& GeometricPlan::refVfk() { return vfk; }

// Other files

/**
 * @brief Replace the other-files vector with @p otherFiles.
 */
void GeometricPlan::setOther(const std::vector<FileOther>& otherFiles)
{
    this->other = otherFiles;
}

/**
 * @brief Append @p otherFile to the back of the other-files vector.
 */
void GeometricPlan::pushOther(const FileOther& otherFile)
{
    this->other.push_back(otherFile);
}

/**
 * @brief Erase all entries from the other-files vector.
 */
void GeometricPlan::clearOther()
{
    this->other.clear();
}

/**
 * @brief Remove every entry equal to @p otherFile from the other-files vector.
 *
 * Uses std::remove + erase (the erase-remove idiom) so all matching
 * occurrences are removed in a single pass.
 */
void GeometricPlan::removeOther(const FileOther& otherFile)
{
    auto it = std::remove(other.begin(), other.end(), otherFile);
    if (it != other.end())
    {
        other.erase(it, other.end());
    }
}

/**
 * @brief Read-only access to the underlying other-files vector.
 */
const std::vector<FileOther>& GeometricPlan::getOther() const
{
    return other;
}

/**
 * @brief Number of entries in the other-files vector.
 */
int GeometricPlan::getOtherSize() const
{
    return static_cast<int>(other.size());
}

/**
 * @brief Bounds-checked access to a single other-file entry.
 * @throws std::out_of_range If @p index is invalid (raised by validateIndex).
 */
const FileOther& GeometricPlan::getOtherAt(int index) const
{
    validateIndex(index, "getOtherAt");
    return other.at(index);
}

/**
 * @brief Bounds-checked removal of a single other-file entry.
 * @throws std::out_of_range If @p index is invalid (raised by validateIndex).
 */
void GeometricPlan::removeOtherAt(int index)
{
    validateIndex(index, "removeOtherAt");
    other.erase(other.begin() + index);
}

// File removal — reset a named slot to its empty/default state via clearAll().

// PDF removers
void GeometricPlan::removePopispole() { popispole.clearAll(); }     // Remove popispole
void GeometricPlan::removeNacrt() { nacrt.clearAll(); }             // Remove nacrt
void GeometricPlan::removeZap() { zap.clearAll(); }                 // Remove zap
void GeometricPlan::removeProt() { prot.clearAll(); }               // Remove prot
void GeometricPlan::removeVymery() { vymery.clearAll(); }           // Remove vymery
void GeometricPlan::removeSezvlast() { sezvlast.clearAll(); }       // Remove sezvlast
void GeometricPlan::removeOprav() { oprav.clearAll(); }             // Remove oprav
void GeometricPlan::removeDsps() { dsps.clearAll(); }               // Remove dsps
void GeometricPlan::removeVytyc() { vytyc.clearAll(); }             // Remove vytyc
void GeometricPlan::removeGnss() { gnss.clearAll(); }               // Remove gnss
void GeometricPlan::removeGp() { gp.clearAll(); }                   // Remove gp
void GeometricPlan::removeZadost() { zadost.clearAll(); }           // Remove zadost
void GeometricPlan::removeOvereni() { overeni.clearAll(); }         // Remove overeni

// Non-PDF removers
void GeometricPlan::removeVfk() { vfk.clearAll(); }                 // Remove vfk
void GeometricPlan::removeSs() { ss.clearAll(); }                   // Remove ss
void GeometricPlan::removeAZI1() { AZI1.clearAll(); }               // Remove AZI1
void GeometricPlan::removeAZI2() { AZI2.clearAll(); }               // Remove AZI2
void GeometricPlan::removeAZI3() { AZI3.clearAll(); }               // Remove AZI3

/**
 * @brief Reset every named slot to empty and clear the other-files vector.
 */
void GeometricPlan::removeAll()
{
    removePopispole();
    removeNacrt();
    removeZap();
    removeProt();
    removeVymery();
    removeSezvlast();
    removeOprav();
    removeDsps();
    removeVytyc();
    removeGnss();
    removeGp();
    removeZadost();
    removeOvereni();

    removeVfk();
    removeSs();
    removeAZI1();
    removeAZI2();
    removeAZI3();

    clearOther();
}
