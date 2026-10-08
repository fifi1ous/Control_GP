/**
 * @file maps.h
 * @brief Mapping of bounding box class IDs to human-readable Czech names.
 *
 * Provides the BB_NAMES constant used by BBoxOverlay to label
 * detected regions on geometric plan PDF pages.
 */

#ifndef MAPS_H
#define MAPS_H

#include <QMap>
#include <unordered_map>
#include <functional>
#include <QStringList>

#include "structures.h"
#include "addpath.h"
#include "filepdf.h"
#include "vfk_maps.h"   // SOBR_MAP / SPOL_MAP / PAR_MAP live here (single source of truth)

/**
 * @brief Configuration and metadata for bounding box detection classes.
 *
 * Maps numeric class IDs to a structured object containing Czech labels,
 * internal abbreviations, and filesystem paths.
 *
 * | ID | Label          | Abbr.     | Translation         | Path/Source                   |
 * |:---|:---------------|:----------|:--------------------|:------------------------------|
 * | 0  | Mapové pole    | map       | Map field           | AddPath::getGPMapPath()       |
 * | 1  | Výkaz výměr    | vykaz     | Area statement      | AddPath::getGPVykazPath()     |
 * | 2  | BPEJ           | bpej      | BPEJ                | AddPath::getGPBpejPath()      |
 * | 3  | SS             | ss        | Comparison assembly | AddPath::getGPSSPath()        |
 * | 4  | Popisové pole  | popispole | Description field   | AddPath::getGPPopispolePath() |
 * | 5  | Klad           | klad      | Sheet index         | AddPath::getGPKladPath()      |
 *
 * Implemented as a function returning a reference to a function-local
 * ``static`` so the underlying ``AddPath::getGPxxxPath()`` calls run on
 * first use — i.e. **after** ``QApplication`` has been constructed. A
 * plain ``const QMap`` at namespace scope would be initialised before
 * ``main()``, triggering ``QCoreApplication::applicationDirPath`` warnings
 * and capturing relative paths. ``inline`` ensures every translation unit
 * shares the same instance instead of building its own copy.
 *
 * @see MapPartsGP
 */
inline const QMap<int, MapPartsGP>& BB_NAMES()
{
    static const QMap<int, MapPartsGP> map = {
        {0, {"Mapové pole", "map", AddPath::getGPMapPath() }},
        {1, {"Výkaz výměr", "vykaz", AddPath::getGPVykazPath() }},
        {2, {"BPEJ", "bpej", AddPath::getGPBpejPath()}},
        {3, {"SS", "ss", AddPath::getGPSSPath()}},
        {4, {"Popisové pole", "popispole", AddPath::getGPPopispolePath()}},
        {5, {"Klad", "klad", AddPath::getGPKladPath() }}
    };
    return map;
}

inline const QMap<QString, QString> NAMESMAP = {
    {"dsps", "DSPS"},
    {"nacrt", "Náčrt"},
    {"gp", "Geometrický plán"},
    {"zadost", "Žádost"},
    {"prot", "Protokol o výpočtech"},
    {"zap", "Zápisník"},
    {"vymery", "Výměry"},
    {"popispole", "Popisové pole"},
    {"sezvlast", "Seznámení vlastníků"},
    {"oprav", "Opravy"},
    {"vytyc", "Dokumentace o vytyčení"}
};


// VFK column-mapping tables (SOBR_MAP, SPOL_MAP, PAR_MAP) are defined in
// vfk_maps.h, which is included above.

#endif // MAPS_H
