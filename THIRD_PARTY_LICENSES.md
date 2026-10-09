# Third-party licenses

The Control_GP source code is licensed under the MIT License (see `LICENSE`).
The model files in `models/` are licensed under AGPL-3.0 (see `models/LICENSE`).
Control_GP uses or redistributes the third-party components listed below. Each component remains
under its own license; the full license texts are available at the linked project pages
and are shipped inside the respective packages (e.g. `*.dist-info/` in `python-embed/`).

## Native components

| Component | Version | License | Link |
|---|---|---|---|
| Qt 6 (Core, Gui, Widgets, Pdf, PdfWidgets) | 6.x | LGPL-3.0-only (or GPL-2.0/GPL-3.0, or commercial) | https://www.qt.io/licensing |
| Tesseract OCR (UB Mannheim build) | 5.5.0 | Apache-2.0 | https://github.com/tesseract-ocr/tesseract |
| Tesseract language data – ces.traineddata | bundled with Tesseract 5.5.0 | Apache-2.0 | https://github.com/tesseract-ocr/tessdata |
| Leptonica | 1.85.0 | BSD-2-Clause | http://www.leptonica.org/ |
| Poppler (poppler-windows build) | 24.08.0 | GPL-2.0-or-later | https://poppler.freedesktop.org/ · https://github.com/oschwartz10612/poppler-windows |
| pybind11 | 2.13.6 | BSD-3-Clause | https://github.com/pybind/pybind11 |
| Python (embeddable package) | 3.13.7 | PSF-2.0 | https://docs.python.org/3/license.html |
| GCC runtime – libgcc_s_seh-1.dll, libstdc++-6.dll | bundled with Tesseract | GPL-3.0 with GCC Runtime Library Exception 3.1 | https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html |
| MinGW-w64 winpthreads – libwinpthread-1.dll | bundled with Tesseract | MIT and BSD-3-Clause | https://www.mingw-w64.org/ |

The Poppler package additionally contains its own dependencies (e.g. cairo, freetype,
libjpeg, libpng, libtiff, zlib, openjpeg); their licenses are listed in that package.

## Python packages (`requirements.txt`)

| Component | Version | License | Link |
|---|---|---|---|
| pypdf | 6.11.0 | BSD-3-Clause | https://github.com/py-pdf/pypdf |
| pdf2image | 1.17.0 | MIT | https://github.com/Belval/pdf2image |
| pillow | 12.2.0 | MIT-CMU | https://python-pillow.github.io |
| onnxruntime | 1.30.0 | MIT | https://onnxruntime.ai |
| opencv-python-headless | 4.13.0.92 | Apache-2.0 | https://github.com/opencv/opencv-python |
| numpy | 2.4.6 | BSD-3-Clause AND 0BSD AND MIT AND Zlib AND CC0-1.0 | https://numpy.org |
| requests | 2.34.2 | Apache-2.0 | https://github.com/psf/requests |

## Transitive Python packages

Collected with `pip-licenses` from `python-embed/`.

| Component | Version | License | Link |
|---|---|---|---|
| certifi | 2026.7.22 | MPL-2.0 | https://github.com/certifi/python-certifi |
| charset-normalizer | 3.5.2 | MIT | https://github.com/jawah/charset_normalizer |
| flatbuffers | 25.12.19 | Apache-2.0 | https://google.github.io/flatbuffers/ |
| idna | 3.20 | BSD-3-Clause | https://github.com/kjd/idna |
| packaging | 26.3 | Apache-2.0 OR BSD-2-Clause | https://github.com/pypa/packaging |
| protobuf | 7.36.2 | BSD-3-Clause | https://developers.google.com/protocol-buffers/ |
| urllib3 | 2.8.0 | MIT | https://github.com/urllib3/urllib3 |

## Models

| Component | Version | License | Link |
|---|---|---|---|
| Ultralytics YOLO (origin of the models in `models/`) | 8.4.52 | AGPL-3.0 | https://github.com/ultralytics/ultralytics |

The model files in `models/` were trained with Ultralytics YOLO and are distributed
under AGPL-3.0 (`models/LICENSE`). Ultralytics itself is not part of the application;
the models are run with onnxruntime.
