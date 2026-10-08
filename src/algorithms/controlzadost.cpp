#include "controlzadost.h"

#include "geometricplan.h"
#include "filepdf.h"

ControlZadost::ControlZadost(GeometricPlan& gp)
    : m_file(&gp.refZadost()) {}
