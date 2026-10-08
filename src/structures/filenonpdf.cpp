#include "filenonpdf.h"

// Constructors
FileNonPDF::FileNonPDF()
    : is_present(false), ku(0), zpmz(0), name(), path(),
    letter(), zpmz_gp_noth(), format() {}

FileNonPDF::FileNonPDF(bool is_present, int ku, int zpmz, const QString& zpmz_gp_noth,
                       const QString& name, const QString& format, const QString& path,
                       const QString& letter)
    : is_present(is_present), ku(ku), zpmz(zpmz), name(name), path(path),
    letter(letter), zpmz_gp_noth(zpmz_gp_noth), format(format) {}

// Setters
void FileNonPDF::setPresent(bool is_present) { this->is_present = is_present; }
void FileNonPDF::setKu(int ku) { this->ku = ku; }
void FileNonPDF::setZpmz(int zpmz) { this->zpmz = zpmz; }
void FileNonPDF::setName(const QString& name) { this->name = name; }
void FileNonPDF::setPath(const QString& path) { this->path = path; }
void FileNonPDF::setLetter(const QString& letter) { this->letter = letter; }
void FileNonPDF::setZpmz_gp_noth(const QString& zpmz_gp_noth) { this->zpmz_gp_noth = zpmz_gp_noth; }
void FileNonPDF::setFormat(const QString& format) { this->format = format; }
void FileNonPDF::setWhatItIs(const QString& what_it_is) { this->what_it_is = what_it_is; }

void FileNonPDF::setAll(bool is_present, int ku, int zpmz, const QString& zpmz_gp_noth,
                        const QString& name, const QString& format, const QString& path,
                        const QString& letter)
{
    this->is_present = is_present;
    this->ku = ku;
    this->zpmz = zpmz;
    this->name = name;
    this->path = path;
    this->letter = letter;
    this->zpmz_gp_noth = zpmz_gp_noth;
    this->format = format;
}

// Getters
bool FileNonPDF::getPresent() const { return is_present; }
int FileNonPDF::getKu() const { return ku; }
int FileNonPDF::getZpmz() const { return zpmz; }
QString FileNonPDF::getName() const { return name; }
QString FileNonPDF::getPath() const { return path; }
QString FileNonPDF::getLetter() const { return letter; }
QString FileNonPDF::getZpmz_gp_noth() const { return zpmz_gp_noth; }
QString FileNonPDF::getFormat() const { return format; }
QString FileNonPDF::getWhatItIs() const {return what_it_is;}

// Clear all data
void FileNonPDF::clearAll()
{
    is_present = false;
    ku = 0;
    zpmz = 0;
    name.clear();
    path.clear();
    letter.clear();
    zpmz_gp_noth.clear();
    format.clear();
}
