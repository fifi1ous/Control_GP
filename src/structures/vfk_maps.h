/**
 * @file vfk_maps.h
 * @brief Column-index-to-field mapping tables for VFK record parsing.
 *
 * Each map associates a 1-based column index with a lambda that writes
 * the parsed QString value into the corresponding field of a VFK structure.
 * Used by ProcessVFK to populate VfkSObr, VfkSPol, and VfkPar records.
 *
 * These tables are the single source of truth for VFK column mapping;
 * maps.h includes this header rather than redefining them.
 */

#ifndef VFK_MAPS_H
#define VFK_MAPS_H

#include <unordered_map>
#include <functional>
#include "structures.h"
#include <QStringList>

/**
 * @brief Column mapping for VfkSObr (SOBR block) records.
 *
 * Maps semicolon-delimited field indices to the corresponding
 * VfkSObr struct members. Index 1 = id, 2 = stav_dat, ..., 10 = kk.
 */
inline const std::unordered_map<int, std::function<void(VfkSObr&, const QString&)>> SOBR_MAP =
    {
        {1, [](VfkSObr& s, const QString& v) { s.id = v.toInt(); }},
        {2, [](VfkSObr& s, const QString& v) { s.stav_dat = v.toShort(); }},
        {3, [](VfkSObr& s, const QString& v) { s.ku = v.toInt(); }},
        {4, [](VfkSObr& s, const QString& v) { s.zpmz = v.toInt(); }},
        {5, [](VfkSObr& s, const QString& v) { s.tl = v.toInt(); }},
        {6, [](VfkSObr& s, const QString& v) { s.cb = v.toInt(); }},
        {7, [](VfkSObr& s, const QString& v) { s.cb_uplne = v.toInt(); }},
        {8, [](VfkSObr& s, const QString& v) { s.y = v.toDouble(); }},
        {9, [](VfkSObr& s, const QString& v) { s.x = v.toDouble(); }},
        {10, [](VfkSObr& s, const QString& v) { s.kk = v.toShort(); }}
};

/**
 * @brief Column mapping for VfkSPol (SPOL block) records.
 *
 * Maps semicolon-delimited field indices to the corresponding
 * VfkSPol struct members. Extends SOBR with measurement-origin fields
 * at indices 11 (ku_kod_mereni) and 12 (zpmz_cislo_mereni).
 */
inline const std::unordered_map<int, std::function<void(VfkSPol&, const QString&)>> SPOL_MAP =
    {
        {1, [](VfkSPol& s, const QString& v) { s.id = v.toInt(); }},
        {2, [](VfkSPol& s, const QString& v) { s.stav_dat = v.toShort(); }},
        {3, [](VfkSPol& s, const QString& v) { s.ku = v.toInt(); }},
        {4, [](VfkSPol& s, const QString& v) { s.zpmz = v.toInt(); }},
        {5, [](VfkSPol& s, const QString& v) { s.tl = v.toInt(); }},
        {6, [](VfkSPol& s, const QString& v) { s.cb = v.toInt(); }},
        {7, [](VfkSPol& s, const QString& v) { s.cb_uplne = v.toInt(); }},
        {8, [](VfkSPol& s, const QString& v) { s.y = v.toDouble(); }},
        {9, [](VfkSPol& s, const QString& v) { s.x = v.toDouble(); }},
        {10, [](VfkSPol& s, const QString& v) { s.kk = v.toShort(); }},
        {11, [](VfkSPol& s, const QString& v) { s.ku_kod_mereni = v.toInt(); }},
        {12, [](VfkSPol& s, const QString& v) { s.zpmz_cislo_mereni = v.toInt(); }}
};

/**
 * @brief Column mapping for VfkPar (PAR block) records.
 *
 * Maps semicolon-delimited field indices to the corresponding
 * VfkPar struct members. Covers all 30 fields of a parcel record.
 */
inline const std::unordered_map<int, std::function<void(VfkPar&, const QString&)>> PAR_MAP =
    {
        {1,  [](VfkPar& s, const QString& v) { s.id = v.toInt(); }},
        {2,  [](VfkPar& s, const QString& v) { s.stav_dat = v.toShort(); }},
        {3,  [](VfkPar& s, const QString& v) { s.dat_vzniku = v; }},
        {4,  [](VfkPar& s, const QString& v) { s.dat_zaniku = v; }},
        {5,  [](VfkPar& s, const QString& v) { s.priznak_kontextu = v.toShort(); }},
        {6,  [](VfkPar& s, const QString& v) { s.rizeni_id_vzniku = v.toInt(); }},
        {7,  [](VfkPar& s, const QString& v) { s.rizeni_id_zaniku = v.toInt(); }},
        {8,  [](VfkPar& s, const QString& v) { s.pkn_id = v.toInt(); }},
        {9,  [](VfkPar& s, const QString& v) { s.par_type = v.toInt(); }},
        {10, [](VfkPar& s, const QString& v) { s.kat_uz_kod = v.toInt(); }},
        {11, [](VfkPar& s, const QString& v) { s.kat_uz_kod_puv = v.toInt(); }},
        {12, [](VfkPar& s, const QString& v) { s.druh_cislovani = v.toShort(); }},
        {13, [](VfkPar& s, const QString& v) { s.kmenove_cislo_par = v.toInt(); }},
        {14, [](VfkPar& s, const QString& v) { s.zdpace_kod = v.toShort(); }},
        {15, [](VfkPar& s, const QString& v) { s.podeleni_cisla_par = v.toShort(); }},
        {16, [](VfkPar& s, const QString& v) { s.dil_parcely = v.toShort(); }},
        {17, [](VfkPar& s, const QString& v) { s.maplis_kod = v.toInt(); }},
        {18, [](VfkPar& s, const QString& v) { s.zp_ur_vym = v.toShort(); }},
        {19, [](VfkPar& s, const QString& v) { s.druh_poz = v.toShort(); }},
        {20, [](VfkPar& s, const QString& v) { s.zpus_vyuz = v.toInt(); }},
        {21, [](VfkPar& s, const QString& v) { s.typ_parcely = v.toShort(); }},
        {22, [](VfkPar& s, const QString& v) { s.vymera = v.toInt(); }},
        {23, [](VfkPar& s, const QString& v) { s.def_bod = v; }},
        {24, [](VfkPar& s, const QString& v) { s.tel_id = v.toInt(); }},
        {25, [](VfkPar& s, const QString& v) { s.par_id = v.toInt(); }},
        {26, [](VfkPar& s, const QString& v) { s.bud_id = v.toInt(); }},
        {27, [](VfkPar& s, const QString& v) { s.ident_bud = v; }},
        {28, [](VfkPar& s, const QString& v) { s.soucasti = v; }},
        {29, [](VfkPar& s, const QString& v) { s.ps_id = v.toInt(); }},
        {30, [](VfkPar& s, const QString& v) { s.ident_ps = v; }}
};

#endif
