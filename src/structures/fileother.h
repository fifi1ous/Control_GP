/**
 * @file FileOther.h
 * @brief Declaration of the FileOther class for miscellaneous file metadata.
 */

#ifndef FILEOTHER_H
#define FILEOTHER_H

#include <QString>

/**
 * @class FileOther
 * @brief Represents metadata of an additional/miscellaneous file in a geometric plan.
 *
 * Used for files that the geometric plan author included beyond the
 * standard set of PDF and non-PDF documents. Stores basic file
 * identification: name, format, path, and a description of its contents.
 */
class FileOther
{
private:
    QString name;           ///< File name.
    QString format;         ///< File format/extension.
    QString path;           ///< Absolute path to the file on disk.
    QString what_it_is;     ///< Description of what the file contains.

public:
    /**
     * @brief Default constructor.
     */
    FileOther();

    /**
     * @brief Parameterized constructor.
     * @param name        File name.
     * @param format      File format/extension.
     * @param path        Absolute file-system path.
     * @param what_it_is  Description of the file contents.
     */
    FileOther(const QString& name, const QString& format, const QString& path, const QString& what_it_is);

    /** @name Setters
     *  @{ */
    void setName(const QString& name);              ///< Set the file name.
    void setFormat(const QString& format);          ///< Set the file format.
    void setPath(const QString& path);              ///< Set the absolute file-system path.
    void setWhatItIs(const QString& what_it_is);    ///< Set the file description.
    /** @} */

    /** @name Getters
     *  @{ */
    QString getName() const;            ///< Get the file name.
    QString getFormat() const;          ///< Get the file format.
    QString getPath() const;            ///< Get the absolute file-system path.
    QString getWhatItIs() const;        ///< Get the file description.
    /** @} */

    /**
     * @brief Virtual destructor (default).
     */
    virtual ~FileOther() = default;
};

/**
 * @brief Equality operator for FileOther.
 *
 * Two FileOther objects are equal if all four fields
 * (name, format, path, what_it_is) match.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 * @return true if all fields are equal, false otherwise.
 */
inline bool operator==(const FileOther& lhs, const FileOther& rhs)
{
    return lhs.getName() == rhs.getName() &&
           lhs.getFormat() == rhs.getFormat() &&
           lhs.getPath() == rhs.getPath() &&
           lhs.getWhatItIs() == rhs.getWhatItIs();
}

#endif // FILEOTHER_H
