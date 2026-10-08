#include "ocrmanager.h"

#include "tesseract_capi.h"

#include <QImage>
#include <QFileInfo>
#include <QDebug>
#include <QByteArray>

// CONTROL_GP_TESSDATA_PATH is injected from CMake (target_compile_definitions)
// and points at e.g. "C:/Program Files/Tesseract-OCR/tessdata". Hardcoding
// it here as a fallback keeps the file usable if someone forgets to pass
// it on the command line — Tesseract will surface a clear error at init
// time if the directory is wrong.
#ifndef CONTROL_GP_TESSDATA_PATH
#define CONTROL_GP_TESSDATA_PATH "C:/Program Files/Tesseract-OCR/tessdata"
#endif

// Source DPI advertised to Tesseract for the input images. This must match
// the ACTUAL resolution of the images handed to ocrImage(): it drives
// Tesseract's font-size heuristics (not pixel sampling), and a mismatch
// (advertising a different DPI than the image was rendered at) skews those
// heuristics and visibly corrupts the thin plotter-font digits — doubled or
// dropped leading digits on the Y column of the SS.
//
// All rasterisers feed 600 DPI (convert_pdf_to_images default in
// scripts/process_pdf.py; process_ss.py / process_popispole.py /
// process_vymery.py / work_with_pdf.py all pass dpi=600), so this is 600 to
// match them 1:1. 600 (rather than Tesseract's 300 DPI throughput sweet spot)
// is deliberate: the extra resolution measurably improves recognition of the
// thin cadastral plotter font, and accuracy is the priority here. If you ever
// change the rasterisation DPI, change this in lockstep — and keep both high
// for accuracy. If you feed images from another source, set this to their DPI.
static constexpr int kRenderDpi = 600;

// Locked language: every document this app processes is Czech, so we
// avoid the per-call language switch logic and the extra state it'd
// require. If we ever need a second language, reintroduce the
// language-aware ensureInitialized from git history.
static constexpr const char* kTessLanguage = "ces";

// Page segmentation mode. PSM_SINGLE_BLOCK (=6) tells Tesseract to
// treat the input as one uniform block of text instead of running full
// page-layout analysis. The cropped SS popisové pole IS one block, so
// PSM_AUTO (the default, =3) wastes effort on layout detection and
// frequently mis-segments the field into spurious columns.
static constexpr int kPageSegMode = 6;

// Confidence threshold (0..100). Mean recognition confidence below this
// triggers a qWarning so the caller can flag the field for manual
// review. 60 is the value Tesseract documentation recommends as the
// boundary between "probably correct" and "probably garbage".
static constexpr int kMinAcceptableConfidence = 20;

void* OcrManager::s_tessApi = nullptr;

bool OcrManager::ensureInitialized()
{
    if (s_tessApi) {
        return true;
    }

    TessBaseAPI* api = TessBaseAPICreate();
    if (!api) {
        qWarning() << "OcrManager: TessBaseAPICreate vrátilo NULL";
        return false;
    }

    const int rc = TessBaseAPIInit3(api, CONTROL_GP_TESSDATA_PATH, kTessLanguage);
    if (rc != 0) {
        qWarning() << "OcrManager: TessBaseAPIInit3 selhal pro jazyk"
                   << kTessLanguage
                   << "(tessdata:" << CONTROL_GP_TESSDATA_PATH << ")";
        TessBaseAPIDelete(api);
        return false;
    }

    // One-time engine configuration. These persist across SetImage calls
    // because Tesseract holds them on the TessBaseAPI handle, so we set
    // them once at init and don't pay the cost on every page.
    TessBaseAPISetPageSegMode(api, kPageSegMode);

    // Skip the inverted-text detection pass. Cadastral documents are
    // always dark-on-light, so the invert pass is pure overhead (~30%
    // of recognition time on typical SS crops).
    TessBaseAPISetVariable(api, "tessedit_do_invert", "0");

    // Preserve runs of spaces between words. The SS field uses spacing
    // for column alignment; the default collapses them and destroys the
    // visual structure that downstream regex parsing relies on.
    TessBaseAPISetVariable(api, "preserve_interword_spaces", "1");

    // Belt-and-braces DPI hint. SetSourceResolution covers this for the
    // current page, but Tesseract's font-size heuristics also consult
    // this variable in a few internal code paths.
    TessBaseAPISetVariable(api, "user_defined_dpi",
                           QByteArray::number(kRenderDpi).constData());

    // Soft cap on per-image recognition time. A malformed page or a
    // pathological glyph can otherwise wedge the worker indefinitely;
    // 60 s is comfortably above the worst observed legitimate case.
    TessBaseAPISetVariable(api, "tessedit_timeout", "60");

    // NB: the character whitelist/blacklist is deliberately NOT set here.
    // It is per-recognition state (see applyCharset / ocrImage) so a single
    // cached engine can OCR numeric, text and mixed fields in turn.

    s_tessApi = api;
    qDebug() << "OcrManager: Tesseract inicializován pro jazyk" << kTessLanguage
             << "(PSM" << kPageSegMode << ", DPI" << kRenderDpi << ")";
    return true;
}

namespace {

// Push the whitelist/blacklist for the requested charset onto the engine.
// Both variables are always written so switching modes on the cached handle
// can never leave a stale restriction behind: an empty string clears the
// corresponding list in Tesseract.
void applyCharset(TessBaseAPI* api, OcrManager::OcrCharset charset)
{
    const char* whitelist = "";
    const char* blacklist = "";

    switch (charset) {
    case OcrManager::OcrCharset::Numeric_parcel:
        // Digits plus the separators that appear in numeric cadastral fields.
        whitelist = "0123456789.,-/)";
        break;
    case OcrManager::OcrCharset::Numeric_coord:
        // Digits plus the separators that appear in numeric cadastral fields.
        whitelist = "0123456789.,- ";
        break;
    case OcrManager::OcrCharset::Text:
        // Let the ces model emit anything except digits.
        blacklist = "0123456789";
        break;
    case OcrManager::OcrCharset::Alphanumeric:
        // No restriction — both lists cleared.
        break;
    }

    TessBaseAPISetVariable(api, "tessedit_char_whitelist", whitelist);
    TessBaseAPISetVariable(api, "tessedit_char_blacklist", blacklist);
}

} // namespace

QString OcrManager::ocrImage(const QString& image_path, OcrCharset charset)
{
    if (!QFileInfo::exists(image_path)) {
        qWarning() << "OcrManager: obrázek neexistuje:" << image_path;
        return QString();
    }

    if (!ensureInitialized()) {
        return QString();
    }

    QImage image(image_path);
    if (image.isNull()) {
        qWarning() << "OcrManager: nepodařilo se načíst obrázek:" << image_path;
        return QString();
    }

    // Tesseract takes raw pixels (1 byte/pixel grayscale). We build that
    // single channel from the GREEN channel rather than a luminance (Y)
    // conversion. The documents are black text with occasional RED text on a
    // light background, and pure red (255,0,0) has a luminance of only ~76 —
    // mid-grey, which Tesseract's internal thresholding can discard. In the
    // green channel both black (G=0) and red (G=0) map to 0 while the light
    // background stays high, so red text comes out as crisp black instead of
    // washed-out grey. ``gray`` owns a fresh buffer and outlives the
    // SetImage/recognition calls below.
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
    QImage gray(rgb.width(), rgb.height(), QImage::Format_Grayscale8);
    gray.setDotsPerMeterX(rgb.dotsPerMeterX());
    gray.setDotsPerMeterY(rgb.dotsPerMeterY());
    for (int y = 0; y < rgb.height(); ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        uchar*      dstLine = gray.scanLine(y);
        for (int x = 0; x < rgb.width(); ++x)
            dstLine[x] = static_cast<uchar>(qGreen(srcLine[x]));
    }

    TessBaseAPI* api = static_cast<TessBaseAPI*>(s_tessApi);
    TessBaseAPISetImage(
        api,
        gray.constBits(),
        gray.width(),
        gray.height(),
        /*bytes_per_pixel*/ 1,
        /*bytes_per_line */ gray.bytesPerLine());

    TessBaseAPISetSourceResolution(api, kRenderDpi);

    // Restrict the recognised characters for this call. Must happen before
    // GetUTF8Text (which triggers recognition) and is reset every call.
    applyCharset(api, charset);

    char* raw = TessBaseAPIGetUTF8Text(api);
    if (!raw) {
        return QString();
    }
    const QString text = QString::fromUtf8(raw);
    TessDeleteText(raw);

    // MeanTextConf is meaningful only after a recognition pass — i.e.
    // after GetUTF8Text — so we read it here. Logging (rather than
    // returning the value) keeps the public signature stable; if a
    // future caller needs to act on confidence, switch this to an
    // out-parameter or a small struct return.
    const int confidence = TessBaseAPIMeanTextConf(api);
    if (confidence < kMinAcceptableConfidence) {
        qWarning() << "OcrManager: nízká důvěra OCR (" << confidence << "/100)"
                   << "pro" << image_path
                   << "— text může vyžadovat manuální kontrolu";
    } else {
        qDebug() << "OcrManager: OCR" << image_path
                 << "důvěra" << confidence << "/100";
    }

    return text;
}

void OcrManager::finalize()
{
    if (s_tessApi) {
        TessBaseAPIEnd(static_cast<TessBaseAPI*>(s_tessApi));
        TessBaseAPIDelete(static_cast<TessBaseAPI*>(s_tessApi));
        s_tessApi = nullptr;
    }
}
