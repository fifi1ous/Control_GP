/**
 * @file FileNonPDF.h
 * @brief Declaration of the FileNonPDF class for non-PDF file metadata.
 */
#ifndef FILENONPDF_H
#define FILENONPDF_H

#include <QString>

/**
 * @class FileNonPDF
 * @brief Represents metadata of a non-PDF file within a geometric plan.
 *
 * Stores presence status, cadastral identifiers (KU, ZPMZ), naming,
 * file-system path, the ZPMZ/GP/nothing indicator, and the file
 * format/extension.
 */
class FileNonPDF
{
private:
    bool is_present;        ///< Whether the file is present in the submission.
    int ku;                 ///< Cadastral unit number (katastralni uzemi).
    int zpmz;               ///< ZPMZ number (zpusob mereni a zobrazeni).
    QString name;           ///< File name.
    QString path;           ///< Absolute path to the file on disk.
    QString letter;         ///< Letter suffix when multiple GPs share one "nacrt".
    QString zpmz_gp_noth;   ///< Indicates whether the file belongs to ZPMZ, GP, or neither.
    QString format;         ///< File format/extension.
    QString what_it_is;

public:
    FileNonPDF();

    FileNonPDF(bool is_present, int ku, int zpmz, const QString& zpmz_gp_noth,
               const QString& name, const QString& format, const QString& path,
               const QString& letter);

    /** @name Setters
     *  @{ */
    void setPresent(bool is_present);
    void setKu(int ku);
    void setZpmz(int zpmz);
    void setName(const QString& name);
    void setPath(const QString& path);
    void setLetter(const QString& letter);
    void setZpmz_gp_noth(const QString& zpmz_gp_noth);
    void setFormat(const QString& format);
    void setWhatItIs(const QString& what_it_is);

    void setAll(bool is_present, int ku, int zpmz, const QString& zpmz_gp_noth,
                const QString& name, const QString& format, const QString& path,
                const QString& letter);
    /** @} */

    /** @name Getters
     *  @{ */
    bool getPresent() const;
    int getKu() const;
    int getZpmz() const;
    QString getName() const;
    QString getPath() const;
    QString getLetter() const;
    QString getZpmz_gp_noth() const;
    QString getFormat() const;
    QString getWhatItIs() const;
    /** @} */

    void clearAll();
    ~FileNonPDF() = default;
};

#endif // FILENONPDF_H
