#include "filepdf.h"

// Constructors
FilePDF::FilePDF()
    : is_present(false), ku(0), zpmz(0), name(), path(),
    what_it_is(), letter(), num_pages(0), formats(),
    is_PDFA(false), PDF_version(), PDFA_version(),
    zpmz_gp(), version(), can_signature(false), num_signatures(0) {}

FilePDF::FilePDF(bool is_present, int ku, int zpmz, const QString& zpmz_gp,
                 const QString& name, const QString& version, const QString& path,
                 const QString& what_it_is, const QString& letter)
    : is_present(is_present), ku(ku), zpmz(zpmz), name(name), path(path),
    what_it_is(what_it_is), letter(letter), num_pages(0), formats(),
    is_PDFA(false), PDF_version(), PDFA_version(),
    zpmz_gp(zpmz_gp), version(version), can_signature(false), num_signatures(0) {}

// Setters
void FilePDF::setPresent(bool is_present) { this->is_present = is_present; }
void FilePDF::setKu(int ku) { this->ku = ku; }
void FilePDF::setZpmz(int zpmz) { this->zpmz = zpmz; }
void FilePDF::setName(const QString& name) { this->name = name; }
void FilePDF::setPath(const QString& path) { this->path = path; }
void FilePDF::setWhatItIs(const QString& what_it_is) { this->what_it_is = what_it_is; }
void FilePDF::setLetter(const QString& letter) { this->letter = letter; }
void FilePDF::setNumPages(const int num_pages) { this->num_pages = num_pages; }
void FilePDF::setFormats(const std::vector<QString> formats) { this->formats = formats; }
void FilePDF::setPDFA(const bool is_PDFA) { this->is_PDFA = is_PDFA; }
void FilePDF::setPDFVersion(const float PDF_version) { this->PDF_version = PDF_version; }
void FilePDF::setPDFAVersion(const float PDFA_version) { this->PDFA_version = PDFA_version; }
void FilePDF::setZpmz_gp(const QString& zpmz_gp) { this->zpmz_gp = zpmz_gp; }
void FilePDF::setVersion(const QString& version) { this->version = version; }
void FilePDF::setCanSignature(const bool can_signature) { this->can_signature = can_signature; }
void FilePDF::setNumSignatures(const int num_signatures) { this->num_signatures = num_signatures; }

void FilePDF::setAll(bool is_present, int ku, int zpmz, const QString& zpmz_gp,
                     const QString& name, const QString& version, const QString& path,
                     const QString& what_it_is, const QString& letter)
{
    this->is_present = is_present;
    this->ku = ku;
    this->zpmz = zpmz;
    this->name = name;
    this->path = path;
    this->what_it_is = what_it_is;
    this->letter = letter;
    this->zpmz_gp = zpmz_gp;
    this->version = version;
}

// Getters
bool FilePDF::getPresent() const { return is_present; }
int FilePDF::getKu() const { return ku; }
int FilePDF::getZpmz() const { return zpmz; }
QString FilePDF::getName() const { return name; }
QString FilePDF::getPath() const { return path; }
QString FilePDF::getWhatItIs() const { return what_it_is; }
QString FilePDF::getLetter() const { return letter; }
int FilePDF::getNumPages() const { return num_pages; }
std::vector<QString> FilePDF::getFormats() const { return formats; }
bool FilePDF::getPDFA() const { return is_PDFA; }
float FilePDF::getPDFVersion() const { return PDF_version; }
float FilePDF::getPDFAVersion() const { return PDFA_version; }
QString FilePDF::getZpmz_gp() const { return zpmz_gp; }
QString FilePDF::getVersion() const { return version; }
bool FilePDF::getCanSignature() const { return can_signature; }
int FilePDF::getNumSignatures() const { return num_signatures; }

// Clear all data
void FilePDF::clearAll()
{
    is_present = false;
    ku = 0;
    zpmz = 0;
    name.clear();
    path.clear();
    what_it_is.clear();
    letter.clear();
    num_pages = 0;
    formats.clear();
    is_PDFA = false;
    PDF_version=0;
    PDFA_version=0;
    zpmz_gp.clear();
    version.clear();
    can_signature = false;
    num_signatures = 0;
}
