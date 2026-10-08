// Minimal Tesseract C-API declarations for Control_GP.
//
// We deliberately do NOT pull in the full upstream `tesseract/capi.h`
// because the UB Mannheim Windows distribution of Tesseract ships only
// the runtime DLLs — no headers. Reproducing only the few prototypes we
// actually call keeps the project self-contained and lets MSVC link
// against the import library generated from `libtesseract.def`.
//
// All declarations below are stable C-linkage exports from
// libtesseract-5.dll (Tesseract 5.x). If you need to call another
// C-API function, add its prototype here AND its symbol name to
// `libtesseract.def`, then re-run CMake.

#ifndef CONTROL_GP_TESSERACT_CAPI_H
#define CONTROL_GP_TESSERACT_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle. Tesseract's real definition lives in C++ headers we
// can't include; using a forward-declared incomplete type is enough
// because we only ever pass pointers around.
typedef struct TessBaseAPI TessBaseAPI;

TessBaseAPI* TessBaseAPICreate(void);
void         TessBaseAPIDelete(TessBaseAPI* handle);

// Returns 0 on success, -1 on failure. ``datapath`` is the directory
// containing ``*.traineddata`` files; ``language`` is e.g. "ces" or
// "ces+eng". Both arguments may be NULL to use Tesseract's defaults.
int          TessBaseAPIInit3(TessBaseAPI* handle,
                              const char*  datapath,
                              const char*  language);

void         TessBaseAPIEnd(TessBaseAPI* handle);

// Provide raw pixel data. ``bytes_per_pixel`` is 1 for grayscale,
// 3 for RGB, 4 for RGBA. ``bytes_per_line`` is the row stride in bytes
// (Qt's QImage::bytesPerLine() — usually width*bpp rounded up to 4).
void         TessBaseAPISetImage(TessBaseAPI*         handle,
                                 const unsigned char* imagedata,
                                 int                  width,
                                 int                  height,
                                 int                  bytes_per_pixel,
                                 int                  bytes_per_line);

// Hint the source DPI so Tesseract picks reasonable scaling. Without
// this it falls back to a guess based on image size and prints a warning.
void         TessBaseAPISetSourceResolution(TessBaseAPI* handle, int ppi);

// Configure a Tesseract internal variable. Returns nonzero on success.
// Used here to silence the dpi-guess warning when SetSourceResolution
// already covers it, and to tweak segmentation if needed.
int          TessBaseAPISetVariable(TessBaseAPI* handle,
                                    const char*  name,
                                    const char*  value);

// Set the page segmentation mode. ``mode`` is a TessPageSegMode value
// (an int in the C API). The values we care about:
//   3 = PSM_AUTO          — default; full page layout analysis
//   6 = PSM_SINGLE_BLOCK  — assume one uniform block of text (best for
//                           the cropped SS popisové pole)
//   7 = PSM_SINGLE_LINE   — assume a single text line
// See tesseract's publictypes.h for the full enum.
void         TessBaseAPISetPageSegMode(TessBaseAPI* handle, int mode);

// Returns the mean confidence (0..100) of the last recognition pass.
// Call after GetUTF8Text. Useful as a quality signal for the caller.
int          TessBaseAPIMeanTextConf(TessBaseAPI* handle);

// Returns a malloc'd UTF-8 string. Caller MUST free it via TessDeleteText.
char*        TessBaseAPIGetUTF8Text(TessBaseAPI* handle);
void         TessDeleteText(char* text);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CONTROL_GP_TESSERACT_CAPI_H
