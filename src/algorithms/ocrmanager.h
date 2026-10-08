#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QString>

// Thin wrapper around the Tesseract C API. Used as the OCR fallback when
// PythonManager::extract_SS_from_GP gets back an empty string from pypdf
// (i.e. the cropped PDF has no text layer — typically a scanned document).
//
// Locked to Czech ("ces") because every cadastral document this app
// processes is in Czech. The Tesseract handle is created lazily and cached
// for the lifetime of the process: TessBaseAPIInit3 is expensive (loads
// the language model) and we'd otherwise pay that cost on every call.
class OcrManager
{
public:
    // Character set Tesseract is allowed to emit for a given call. Applied
    // per call (the cached engine handle remembers the last setting, so we
    // always reset it) via tessedit_char_whitelist / _blacklist.
    enum class OcrCharset {
        Alphanumeric,   // No restriction — let the ces model decide (default).
        Numeric_parcel, // Digits and numeric separators only ("0123456789.,-/ ").
        Numeric_coord, // Digits and selected numeric separators
        Text            // Letters only — digits suppressed via the blacklist.
    };

    // OCR a single image file (PNG, JPEG, …) loaded from disk. Returns
    // "" on any failure (missing file, init failure, unreadable image)
    // and logs via qWarning so the caller doesn't need a separate error
    // path. Used by PythonManager::extract_SS_from_GP after the Python
    // helper has rasterised the cropped PDF pages to PNGs.
    //
    // @param charset Restricts the recognised characters; defaults to
    //                Alphanumeric (no restriction).
    static QString ocrImage(const QString& image_path,
                            OcrCharset charset = OcrCharset::Alphanumeric);

    // Release the cached Tesseract handle. Safe to call from app shutdown
    // alongside PythonManager::finalize().
    static void finalize();

private:
    // Lazily creates the Tesseract handle on first call. Returns false if
    // init failed (most commonly because ces.traineddata isn't in the
    // tessdata directory).
    static bool ensureInitialized();

    // Opaque ``TessBaseAPI*``. Stored as void* so the header doesn't
    // need to expose ``tesseract_capi.h`` to every translation unit
    // that includes ``ocrmanager.h``.
    static void* s_tessApi;
};

#endif // OCRMANAGER_H
