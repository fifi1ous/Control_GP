# Control_GP

Desktopová aplikace pro kontrolu podkladů katastru nemovitostí. Kontroluje
**geometrické plány**, **záznamy podrobného měření změn (ZPMZ)** a **žádosti**
proti datům výměnného formátu katastru (**VFK**).

Aplikace načte PDF dokumenty a VFK, klasifikuje a segmentuje stránky pomocí modelů
YOLO (ONNX, onnxruntime), čte text přes OCR (Tesseract) a porovná vytěžené údaje
s VFK. Výsledky zobrazí v prohlížeči PDF se zvýrazněnými oblastmi.

## Požadavky

- Windows 10/11 x64
- Visual Studio 2022 (MSVC, C++17)
- Qt 6 s modulem Qt PDF (Widgets, Pdf, PdfWidgets), ideálně s Qt Creatorem
- CMake 3.16+
- Python 3.13 (plná instalace s hlavičkami a knihovnami, potřebná pro build)
- Tesseract 5 (build UB Mannheim) s `ces.traineddata`, výchozí cesta
  `C:\Program Files\Tesseract-OCR`

## Sestavení

1. **Stáhněte závislosti** (embeddable Python s balíčky z `requirements.txt` a Poppler):

   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\setup_deps.ps1
   ```

2. **Nakonfigurujte projekt.** Otevřete `CMakeLists.txt` v Qt Creatoru a zvolte kit
   s MSVC 2022 x64, nebo z příkazové řádky (Developer PowerShell for VS 2022):

   ```powershell
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
   ```

   Pokud je Tesseract jinde než ve výchozí cestě, přidejte
   `-DTESSERACT_INSTALL_DIR=<cesta>`.

3. **Sestavte:**

   ```powershell
   cmake --build build
   ```

   Po sestavení se vedle `Control_GP.exe` zkopírují Qt knihovny (windeployqt), DLL
   Tesseractu, `tessdata/ces.traineddata`, `python-embed/`, `poppler/`, `scripts/`, `models/`, `database/`
   a `workspace/`. Kopírování lze vypnout volbou `-DCONTROL_GP_DEPLOY=OFF`.

## Struktura repozitáře

```
src/                    C++ zdrojové kódy (app, ui, algorithms, structures)
scripts/                Python skripty volané z aplikace (pybind11::embed)
Icons/                  ikony a icons.qrc
models/                 modely YOLO exportované do ONNX
database/               SQLite databáze (katastrální území, ÚOZI)
third_party/tesseract/  hlavička C API a .def pro generování import knihovny
tools/setup_deps.ps1    stažení závislostí podle deps.lock
workspace/              pracovní složka aplikace (obsah se neverzuje)
deps.lock               verze, URL a SHA256 stahovaných závislostí
requirements.txt        Python balíčky pro python-embed
sync_dir.cmake          inkrementální synchronizace složek (robocopy)
```

Složky `python-embed/`, `poppler/` a `.deps-cache/` vytváří `setup_deps.ps1`
a do repozitáře nepatří.

## Licence

- **Kód aplikace** (vše mimo složku `models/`) je licencován pod
  [MIT License](LICENSE).
- **Modely** ve složce `models/` vznikly v Ultralytics YOLO a mají vlastní licenci
  [GNU AGPL-3.0](models/LICENSE). Ta se vztahuje pouze na soubory v `models/`.

Licence použitých komponent třetích stran jsou v
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## Autor

Filip Roučka, katedra geomatiky, Fakulta stavební ČVUT v Praze
