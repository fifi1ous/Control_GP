#ifndef PROCESSVFK_H
#define PROCESSVFK_H

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDebug>
#include "structures.h"

class ProcessVFK
{
public:
    ProcessVFK();

    // Parse the given VFK file and build the coordinate list.
    // Replaces any data from a previous call.
    static void process(const QString &filePath);

    static const std::vector<VfkSObr> &getSOBR();
    static const std::vector<VfkSPol> &getSPOL();
    static const std::vector<VfkPar> &getPAR();
    static const std::vector<Coordinates> &getSS();
    static const std::vector<Vymery> &getVymeryOld();
    static const std::vector<Vymery> &getVymeryNew();
    static void clear();

private:
    static std::vector<VfkSObr> s_obr;
    static std::vector<VfkSPol> s_pol;
    static std::vector<VfkPar> par;
    static std::vector<Coordinates> ss;
    static std::vector<Vymery> vymeryOld;
    static std::vector<Vymery> vymeryNew;

    static void loadVFK(const QString &filePath);

    static void parseSObr(const QStringList &list);
    static void parseSPol(const QStringList &list);
    static void parsePar(const QStringList &list);

    static void createCoordinates();
    static void createArea();

    static QString getLandTypeName(const short &code);
};

#endif // PROCESSVFK_H
