#ifndef PARSETEXT_H
#define PARSETEXT_H

#include <QStringList>
#include <QRegularExpression>

#include "structures.h"
class ParseText
{
public:
    ParseText();

    static std::vector<Coordinates> parseSSTextOCR(const QString& numeric, const QString& alphaNumeric);

    static std::vector<Coordinates> parseSSTextPDF(const QString& text);

    static Coordinates parseSSTextProt(const QString& text);

    static std::vector<Coordinates> parseSSTextProtPasted(const QString& text);

    static void parseVymeryTextOcr(const QString& numeric, const QString& alphanumeric,
                                   std::vector<Vymery>& vymeryOld,
                                   std::vector<Vymery>& vymeryNew,
                                   std::vector<Vymery>& vymeryPor);

private:

};

#endif // PARSETEXT_H
