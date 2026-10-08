#ifndef CONTROLZADOST_H
#define CONTROLZADOST_H

#include <QString>

class FilePDF;
class GeometricPlan;

/**
 * @class ControlZadost
 * @brief Controller for the Žádost (request) PDF.
 *
 * Does not own its file data — it operates on the @c zadost slot of a
 * GeometricPlan, which is the single source of truth. The controller
 * binds to that slot at construction and forwards all operations to it.
 */
class ControlZadost
{
public:
    /**
     * @brief Bind the controller to the @c zadost slot of @p plan.
     * @param plan GeometricPlan that owns the file data.
     */
    explicit ControlZadost(GeometricPlan& gp);

private:
    FilePDF* m_file;    ///< Points to the zadost slot inside the owning GeometricPlan.
};

#endif // CONTROLZADOST_H
