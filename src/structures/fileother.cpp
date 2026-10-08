#include "fileother.h"

// Constructors
FileOther::FileOther() {}

FileOther::FileOther(const QString& name, const QString& format,
                    const QString& path, const QString& what_it_is)
    : name(name), format(format), path(path), what_it_is(what_it_is) {}

// Setters
void FileOther::setName(const QString& name) { this->name = name; }                       // Set the name of the file
void FileOther::setFormat(const QString& format) { this->format = format; }               // Set the format of the file
void FileOther::setPath(const QString& path) { this->path = path; }                       // Set the path of the file
void FileOther::setWhatItIs(const QString& what_it_is) { this->what_it_is = what_it_is; } // Set what it is

// Getters
QString FileOther::getName() const { return name; }             // Get the name of the file
QString FileOther::getFormat() const { return format; }         // Get the format of the file
QString FileOther::getPath() const { return path; }             // Get the path of the file
QString FileOther::getWhatItIs() const { return what_it_is; }   // Get what it is
