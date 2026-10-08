/**
 * @file FilePDF.h
 * @brief Declaration of the FilePDF class for PDF file metadata.
 */
#ifndef FILEPDF_H
#define FILEPDF_H

#include <QString>
#include <vector>

/**
 * @class FilePDF
 * @brief Represents metadata of a PDF file within a geometric plan.
 *
 * Stores presence status, cadastral identifiers (KU, ZPMZ), naming,
 * file-system path, PDF/PDF-A version information, and the
 * PDF-specific ZPMZ/GP indicator and version string.
 */
class FilePDF
{
private:
    bool is_present;        ///< Whether the file is present in the submission.
    int ku;                 ///< Cadastral unit number (katastralni uzemi).
    int zpmz;               ///< ZPMZ number (zpusob mereni a zobrazeni).
    QString name;           ///< File name.
    QString path;           ///< Absolute path to the file on disk.
    QString what_it_is;     ///< Description when the file is not what it should be.
    QString letter;         ///< Letter suffix when multiple GPs share one "nacrt".
    int num_pages;
    std::vector<QString> formats;
    bool is_PDFA;
    float PDF_version;
    float PDFA_version;
    QString zpmz_gp;        ///< Indicates whether the file belongs to ZPMZ or GP.
    QString version;        ///< PDF version string.
    bool can_signature;     ///< Whether the PDF can hold/accept a signature.
    int num_signatures;     ///< Number of signatures present in the PDF.

public:
    FilePDF();

    FilePDF(bool is_present, int ku, int zpmz, const QString& zpmz_gp,
            const QString& name, const QString& version, const QString& path,
            const QString& what_it_is, const QString& letter);

    /** @name Setters
     *  @{ */
    void setPresent(bool is_present);
    void setKu(int ku);
    void setZpmz(int zpmz);
    void setName(const QString& name);
    void setPath(const QString& path);
    void setWhatItIs(const QString& what_it_is);
    void setLetter(const QString& letter);
    void setNumPages(const int num_pages);
    void setFormats(const std::vector<QString> formats);
    void setPDFA(const bool is_PDFA);
    void setPDFVersion(const float PDF_version);
    void setPDFAVersion(const float PDFA_version);
    void setZpmz_gp(const QString& zpmz_gp);
    void setVersion(const QString& version);
    void setCanSignature(const bool can_signature);
    void setNumSignatures(const int num_signatures);

    void setAll(bool is_present, int ku, int zpmz, const QString& zpmz_gp,
                const QString& name, const QString& version, const QString& path,
                const QString& what_it_is, const QString& letter);
    /** @} */

    /** @name Getters
     *  @{ */
    bool getPresent() const;
    int getKu() const;
    int getZpmz() const;
    QString getName() const;
    QString getPath() const;
    QString getWhatItIs() const;
    QString getLetter() const;
    int getNumPages() const;
    std::vector<QString> getFormats() const;
    bool getPDFA() const;
    float getPDFVersion() const;
    float getPDFAVersion() const;
    QString getZpmz_gp() const;
    QString getVersion() const;
    bool getCanSignature() const;
    int getNumSignatures() const;
    /** @} */

    void clearAll();
    ~FilePDF() = default;
};

#endif // FILEPDF_H
