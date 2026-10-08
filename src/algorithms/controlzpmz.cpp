#include "controlzpmz.h"

#include "geometricplan.h"
#include "filepdf.h"
#include "filenonpdf.h"

ControlZPMZ::ControlZPMZ(GeometricPlan& gp)
    : m_gp(&gp) {}

FilePDF& ControlZPMZ::pdf(FileType type)
{
    switch (type) {
    case Popispole: return m_gp->refPopispole();
    case Nacrt:     return m_gp->refNacrt();
    case Zap:       return m_gp->refZap();
    case Prot:      return m_gp->refProt();
    case Vymery:    return m_gp->refVymery();
    case Sezvlast:  return m_gp->refSezvlast();
    case Oprav:     return m_gp->refOprav();
    case Dsps:      return m_gp->refDsps();
    case Vytyc:     return m_gp->refVytyc();
    case GNSS:      return m_gp->refGnss();
    // Vfk is a FileNonPDF, not a PDF slot — callers must use vfk() for it.
    default:        return m_gp->refPopispole();
    }
}

const FilePDF& ControlZPMZ::pdf(FileType type) const
{
    return const_cast<ControlZPMZ*>(this)->pdf(type);
}

FileNonPDF& ControlZPMZ::vfk()
{
    return m_gp->refVfk();
}

const FileNonPDF& ControlZPMZ::vfk() const
{
    return m_gp->refVfk();
}

QString ControlZPMZ::getFilePath(FileType type) const
{
    if (type == Vfk)
        return m_gp->refVfk().getPath();
    return pdf(type).getPath();
}

void ControlZPMZ::clearFile(FileType type)
{
    if (type == Vfk)
        m_gp->refVfk().clearAll();
    else
        pdf(type).clearAll();
}
