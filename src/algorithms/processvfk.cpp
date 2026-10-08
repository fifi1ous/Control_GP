#include "processvfk.h"

#include "vfk_maps.h"


ProcessVFK::ProcessVFK(){}

// Definitions of the static data members declared in processvfk.h.
std::vector<VfkSObr> ProcessVFK::s_obr;
std::vector<VfkSPol> ProcessVFK::s_pol;
std::vector<VfkPar> ProcessVFK::par;
std::vector<Coordinates> ProcessVFK::ss;
std::vector<Vymery> ProcessVFK::vymeryOld;
std::vector<Vymery> ProcessVFK::vymeryNew;

enum class Command { Unknown, parseSObr, parseSPol, parsePar };

Command getCommand(const QString& str) {
    const QMap<QString, Command> stringToEnum = {
        {"&DSOBR", Command::parseSObr},
        {"&DSPOL", Command::parseSPol},
        {"&DPAR",    Command::parsePar}
    };
    return stringToEnum.value(str, Command::Unknown);
}

void ProcessVFK::process(const QString &filePath)
{
    s_obr.clear();
    s_pol.clear();
    par.clear();
    ss.clear();

    loadVFK(filePath);
    createCoordinates();
    createArea();
}

void ProcessVFK::loadVFK(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open file:" << filePath;
        return;
    }

    QTextStream in(&file);

    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList parts = line.split(";");

        if (!parts.isEmpty()) {
            switch (getCommand(parts[0])) {
            case Command::parseSObr:
                parseSObr(parts);
                break;
            case Command::parseSPol:
                parseSPol(parts);
                break;
            case Command::parsePar:
                parsePar(parts);
                break;
            default:
                break;
            }
        }
    }

    file.close();
}

void ProcessVFK::parseSObr(const QStringList &list)
{
    VfkSObr sobr;

    for (int i = 0; i < list.size(); ++i)
    {
        auto it = SOBR_MAP.find(i);
        if (it != SOBR_MAP.end())
        {
            it->second(sobr, list[i]);
        }
    }

    s_obr.push_back(sobr);
}

void ProcessVFK::parseSPol(const QStringList &list)
{
    VfkSPol spol;

    for (int i = 0; i < list.size(); ++i)
    {
        auto it = SPOL_MAP.find(i);
        if (it != SPOL_MAP.end())
        {
            it->second(spol, list[i]);
        }
    }

    s_pol.push_back(spol);
}

void ProcessVFK::parsePar(const QStringList &list)
{
    VfkPar par_line;

    for (int i = 0; i < list.size(); ++i)
    {
        auto it = PAR_MAP.find(i);
        if (it != PAR_MAP.end())
        {
            it->second(par_line, list[i]);
        }
    }

    par.push_back(par_line);
}

const std::vector<VfkSObr> &ProcessVFK::getSOBR()
{
    return s_obr;
}

const std::vector<VfkSPol> &ProcessVFK::getSPOL()
{
    return s_pol;
}

const std::vector<VfkPar> &ProcessVFK::getPAR()
{
    return par;
}

const std::vector<Coordinates> &ProcessVFK::getSS()
{
    return ss;
}

const std::vector<Vymery> &ProcessVFK::getVymeryOld()
{
    return vymeryOld;
}

const std::vector<Vymery> &ProcessVFK::getVymeryNew()
{
    return vymeryNew;
}

void ProcessVFK::createCoordinates()
{
    for (std::size_t i = 0; i < s_obr.size(); ++i)
    {
        Coordinates coordinate;

        coordinate.cb_full = s_obr[i].cb_uplne;
        coordinate.id      = s_obr[i].cb;

        coordinate.Y1      = s_obr[i].y;
        coordinate.X1      = s_obr[i].x;
        coordinate.kv1     = s_obr[i].kk;

        for (const VfkSPol spol: s_pol)
        {
            if(spol.cb_uplne == s_obr[i].cb_uplne)
            {
                coordinate.Y2      = spol.y;
                coordinate.X2      = spol.x;
                coordinate.kv2     = spol.kk;
            }
        }

        ss.push_back(coordinate);
    }
}

void ProcessVFK::createArea()
{
    for (int i = 0; i < par.size(); i++)
    {
        Vymery vymeraOld;
        Vymery vymeraNew;

        if (par[i].stav_dat == 0)
        {
            vymeraOld.stav_dat = 1;
            if (par[i].druh_cislovani==1){
                vymeraOld.cislovani = "st.";
            }
            else
            {
                vymeraOld.cislovani = "";
            }
            vymeraOld.kmenove   = par[i].kmenove_cislo_par;
            vymeraOld.poddeleni = par[i].podeleni_cisla_par;
            vymeraOld.vymera    = par[i].vymera;
            vymeraOld.zpus_urc = par[i].zp_ur_vym;
            vymeraOld.druh_poz  = getLandTypeName(par[i].druh_poz);
            vymeryOld.push_back(vymeraOld);
        }
        else
        {
            vymeraNew.stav_dat = 2;
            if (par[i].druh_cislovani==1){
                vymeraNew.cislovani = "st.";
            }
            else
            {
                vymeraNew.cislovani = "";
            }
            vymeraNew.kmenove   = par[i].kmenove_cislo_par;
            vymeraNew.poddeleni = par[i].podeleni_cisla_par;
            vymeraNew.vymera    = par[i].vymera;
            vymeraNew.zpus_urc = par[i].zp_ur_vym;
            vymeraNew.druh_poz  = getLandTypeName(par[i].druh_poz);
            vymeryNew.push_back(vymeraNew);
        }

    }
}

QString ProcessVFK::getLandTypeName(const short &code) {
    switch (code) {
    case 2:  return QStringLiteral("orná půda");
    case 3:  return QStringLiteral("chmelnice");
    case 4:  return QStringLiteral("vinice");
    case 5:  return QStringLiteral("zahrada");
    case 6:  return QStringLiteral("ovocný sad");
    case 7:  return QStringLiteral("trvalý travní porost");
    case 10: return QStringLiteral("lesní pozemek");
    case 11: return QStringLiteral("vodní plocha");
    case 13: return QStringLiteral("zastavěná plocha a nádvoří");
    case 14: return QStringLiteral("ostatní plocha");
    default: return QStringLiteral(""); // Fallback for unknown codes
    }
}

void ProcessVFK::clear()
{
    s_obr.clear();
    s_pol.clear();
    par.clear();
    ss.clear();
    vymeryOld.clear();
    vymeryNew.clear();
}