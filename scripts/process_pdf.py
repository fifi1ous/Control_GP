import re
import os
import copy
import struct
import sys
from pypdf import PdfReader, PdfWriter
from pypdf.generic import RectangleObject
from pdf2image import convert_from_path
from PIL import ImageOps


_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_APP_DIR = os.path.dirname(_SCRIPT_DIR)
POPPLER_BIN = os.path.join(_APP_DIR, "poppler", "Library", "bin")

# Fallback to None if the bundled directory doesn't exist — pdf2image will
# then look in PATH (useful during local development without bundled poppler).
if not os.path.isdir(POPPLER_BIN):
    POPPLER_BIN = None


class ProcessPDF:
    _PDFA_DESCRIPTIONS = {
        "1A": "PDF/A-1a",
        "1B": "PDF/A-1b",
        "2A": "PDF/A-2a",
        "2B": "PDF/A-2b",
        "2U": "PDF/A-2u",
        "3A": "PDF/A-3a",
        "3B": "PDF/A-3b",
        "3U": "PDF/A-3u",
        "4":  "PDF/A-4",
        "4E": "PDF/A-4e",
        "4F": "PDF/A-4f",
    }

    _PDF_SIZES = {
        "A0": (2383.94, 3370.39),
        "A1": (1683.78, 2383.94),
        "A2": (1190.55, 1683.78),
        "A3": (841.89, 1190.55),
        "A4": (595.28, 841.89),
        "A5": (419.53, 595.28),
        "A6": (297.64, 419.53),
    }

    def __init__(self, file_path: str):
        self._file_path = file_path
        self._min_version = (1, 0)
        self._features_found = []

    def _read_raw(self) -> bytes:
        with open(self._file_path, "rb") as f:
            return f.read()
        
    def _get_header_version(self, content: bytes) -> tuple[str, tuple[int, int]]:
        header = content[:16] # Only need the beginning to check header
        m = re.search(rb"%PDF-(\d+)\.(\d+)", header)
        if m:
            major = int(m.group(1))
            minor = int(m.group(2))
            return f"{major}.{minor}", (major, minor)
        return "Unknown", (0, 0)
    
    def _get_all_header_versions(self, content: bytes) -> list[str]:
        matches = re.findall(rb"%PDF-(\d+\.\d+)", content)
        return list(set(m.decode() for m in matches))
    
    def _get_catalog_version(self, reader: PdfReader) -> str | None:
        try:
            catalog = reader.trailer.get("/Root")
            if catalog and "/Version" in catalog:
                return str(catalog["/Version"]).lstrip("/")
        except Exception:
            pass
        return None
    
    def _bump(self, version_tuple: tuple, label: str):
        self._features_found.append((label, f"{version_tuple[0]}.{version_tuple[1]}"))
        if version_tuple > self._min_version:
            self._min_version = version_tuple
    
    def _infer_version_from_features(self, content: bytes) -> dict:
        # ── PDF 1.1 ──
        if b"/Encrypt" in content:
            self._bump((1, 1), "Encryption dictionary (/Encrypt)")

        # ── PDF 1.4 ──
        if b"/JBIG2Decode" in content:
            self._bump((1, 4), "JBIG2 image compression (/JBIG2Decode)")
        if re.search(rb"/S\s*/Transparency", content):
            self._bump((1, 4), "Transparency group (/S /Transparency)")

        # ── PDF 1.5 ──
        if re.search(rb"/Type\s*/ObjStm", content):
            self._bump((1, 5), "Object streams (/Type /ObjStm)")
        if re.search(rb"/Type\s*/XRef", content):
            self._bump((1, 5), "Cross-reference streams (/Type /XRef)")
        if b"/JPXDecode" in content:
            self._bump((1, 5), "JPEG2000 compression (/JPXDecode)")
        if b"/OCProperties" in content:
            self._bump((1, 5), "Optional content layers (/OCProperties)")

        # ── PDF 1.6 ──
        if re.search(rb"AESV2|/StdCF", content):
            self._bump((1, 6), "AES-128 encryption (AESV2 / StdCF)")

        # ── PDF 1.7 ──
        if b"/Collection" in content:
            self._bump((1, 7), "PDF Portfolio/Collection (/Collection)")

        # ── PDF 2.0 ──
        if b"AESV3" in content:
            self._bump((2, 0), "AES-256 encryption (AESV3, PDF 2.0)")

        return {
            "inferred_min": f"{self._min_version[0]}.{self._min_version[1]}",
            "inferred_tuple": self._min_version,
            "features": self._features_found,
        }
    
    def _get_pdfa_info(self, content: bytes, reader: PdfReader | None = None) -> dict:
        part = None
        conformance = None

        # Primary source: pypdf resolves the pdfaid namespace by its URI, so it
        # is robust against non-standard XMP prefixes (e.g. "pa:" instead of
        # "pdfaid:") that a raw byte scan would miss.
        if reader is not None:
            try:
                xmp = reader.xmp_metadata
                if xmp is not None:
                    part = xmp.pdfaid_part
                    conformance = xmp.pdfaid_conformance
            except Exception:
                pass

        # Fallback: scan the raw XMP bytes directly. PDF/A mandates an
        # uncompressed document-level metadata stream, so the marker is present
        # in plain text and a regex over the file bytes is reliable here.
        if not part:
            part_match = re.search(rb"<pdfaid:part>\s*(\d+)\s*</pdfaid:part>|pdfaid:part=[\"'](\d+)[\"']", content)
            if part_match:
                part = (part_match.group(1) or part_match.group(2)).decode()
                conf_match = re.search(rb"<pdfaid:conformance>\s*([A-Z]+)\s*</pdfaid:conformance>|pdfaid:conformance=[\"']([A-Z]+)[\"']", content)
                if conf_match:
                    conformance = (conf_match.group(1) or conf_match.group(2)).decode()

        if not part:
            return {"is_pdfa": False, "part": None, "conformance": None, "description": None}

        part = str(part)
        conformance = str(conformance) if conformance else None

        key = f"{part}{conformance}" if conformance else part
        description = self._PDFA_DESCRIPTIONS.get(key, f"PDF/A-{key} (unknown variant)")

        return {"is_pdfa": True, "part": part, "conformance": conformance, "description": description}

    def _get_effective_version(
        self,
        header_version: str,
        catalog_version: str | None,
        inferred_info: dict,
    ) -> str:
        versions: list[tuple[tuple[int, int], str]] = []
        for v in [header_version, catalog_version, inferred_info.get("inferred_min")]:
            if v and v != "Unknown":
                try:
                    parts = tuple(int(x) for x in v.split("."))
                    versions.append((parts, v))  # type: ignore[arg-type]
                except ValueError:
                    pass
        if not versions:
            return "Unknown"
        return max(versions, key=lambda x: x[0])[1]
    
    def _pdf_pages_size(self, reader: PdfReader) -> list[str | None]:
        page_sizes = []
        for i, page in enumerate(reader.pages):
            mb = page.mediabox
            w, h = float(mb.width), float(mb.height)
            page_sizes.append(self._pdf_size_name(w, h))
        return page_sizes

    def _pdf_size_name(self, width: float, height: float) -> str | None:
        # Sort the incoming dimensions so the smaller value is always first
        page_min, page_max = sorted([width, height])
        
        for name, (w, h) in self._PDF_SIZES.items():
            # Your dictionary values (w, h) are already sorted as (smaller, larger)
            if abs(page_min - w) < 50 and abs(page_max - h) < 50:
                return name
                
        return None

    def _get_signature_info(self, content: bytes, reader: PdfReader) -> dict:
        signed_count = 0
        try:
            fields = reader.get_fields() or {}
            for field in fields.values():
                if str(field.get("/FT", "")) == "/Sig" and field.get("/V"):
                    signed_count += 1
        except Exception:
            pass

        docmdp_match = re.search(rb"/DocMDP.*?/P\s+(\d)", content, re.DOTALL)
        docmdp_level = int(docmdp_match.group(1)) if docmdp_match else None

        return {
            "signed_count": signed_count,
            "docmdp_level": docmdp_level,
        }

    def _can_add_signature_field(self, sig_info: dict, pdfa_info: dict) -> str:
        blockers = []
        level = sig_info.get("docmdp_level")

        if level == 1:
            blockers.append(
                "DocMDP /P=1: Dokument nesmí být po podpisu nijak změněn (nelze přidat další podpis)"
            )
        if pdfa_info.get("is_pdfa"):
            part = int(pdfa_info.get("part", 0) or 0)
            if part == 1:
                blockers.append(
                    "PDF/A-1: Archivní formát — přidání podpisu může být omezeno kompatibilitou nástrojů"
                )

        return "; ".join(blockers) if blockers else "Povoleno"

    def _get_signature_capability(self, sig_info: dict, pdfa_info: dict) -> tuple[int, str]:
        reason = self._can_add_signature_field(sig_info, pdfa_info)
        return sig_info.get("signed_count", 0), reason

    def get_pdf_info(self) -> dict:
        """Returns structured information about the PDF."""
        # Read the file once to avoid redundant I/O
        content = self._read_raw()
        reader = PdfReader(self._file_path)
        
        header_version, _ = self._get_header_version(content)
        catalog_version = self._get_catalog_version(reader)
        inferred_info = self._infer_version_from_features(content)
        pdfa_info = self._get_pdfa_info(content, reader)
        sig_info = self._get_signature_info(content, reader)

        sig_count, sig_reason = self._get_signature_capability(sig_info, pdfa_info)

        return {
            "effective_version": self._get_effective_version(header_version, catalog_version, inferred_info),
            "pdfa": {
                "is_pdfa": pdfa_info.get("is_pdfa"),
                "part": pdfa_info.get("part"),
                "conformance": pdfa_info.get("conformance"),
                "description": pdfa_info.get("description"),
            },
            "signatures": {
                "count": sig_count,
                "capability": sig_reason,
            },
            "page_sizes": self._pdf_pages_size(reader),
        }
    
    def extract_text(self, path_to_pdf: str | None = None, type: str = "plain") -> str:
        """Extract text from the PDF using pypdf's layout extraction mode.
        """
        target_path = path_to_pdf or self._file_path
        if not os.path.isfile(target_path):
            raise FileNotFoundError(f"PDF nenalezen: {target_path}")

        reader = PdfReader(target_path)
        page_texts: list[str] = []

        for page in reader.pages:
            text = page.extract_text(extraction_mode=type)


            if text.strip():
                page_texts.append(text)

        return "\n\n".join(page_texts)

        
    def extract_text_into_pages(self, path_to_pdf: str | None = None, type="plain") -> list[str]:
        target_path = path_to_pdf or self._file_path
        if not os.path.isfile(target_path):
            raise FileNotFoundError(f"PDF nenalezen: {target_path}")

        reader = PdfReader(target_path)
        page_texts: list[str] = []

        for page in reader.pages:
            # 1. Extract the text from the actual page object
            text = page.extract_text(extraction_mode=type)

            # 2. Append the string directly if it's not empty
            if text and text.strip():
                page_texts.append(text)
            else:
                # Optional: Keeps an empty string for blank pages to preserve page indexing
                page_texts.append("") 

        return page_texts

    def divide_pdf(self, size: str = "A4", path_to_pdf: str | None = None, save_path: str | None = None) -> PdfWriter:
        """Divide the PDF into smaller pages."""
        target_path = path_to_pdf or self._file_path
        reader = PdfReader(target_path)
        writer = PdfWriter()

        pdf_size = self._PDF_SIZES.get(size.upper())
        if not pdf_size:
            raise ValueError(f"Unsupported size '{size}'")

        max_width, max_height = pdf_size

        for i, page in enumerate(reader.pages):
            self._divide_page(i, page, max_width, max_height, reader, writer)

        if save_path:
            base_name = os.path.basename(target_path)
            name, ext = os.path.splitext(base_name)
            output_file = os.path.join(save_path, f"{name}_{size}{ext}")
            
            # Create directories if they don't exist
            os.makedirs(save_path, exist_ok=True)
            
            with open(output_file, "wb") as f:
                writer.write(f)

        return writer

    def _divide_page(self, page_number: int, page, max_width: float, max_height: float, reader: PdfReader, writer: PdfWriter):
        """Normalize page rotation and recursively split its full area."""
        page = self._normalize_rotation(page)
        media = page.mediabox
        full_rect = (
            float(media.left),
            float(media.bottom),
            float(media.right),
            float(media.top),
        )
        self._split_page(page_number, full_rect, max_width, max_height, reader, writer)

    def _split_page(self, page_number: int, clip_rect: tuple, max_width: float, max_height: float, reader: PdfReader, writer: PdfWriter):
        """Recursively split page region until it fits."""
        x0, y0, x1, y1 = clip_rect
        w = x1 - x0
        h = y1 - y0

        if (w <= max_width + 2 and h <= max_height + 2) or (w <= max_height + 2 and h <= max_width + 2):
            new_page = copy.copy(reader.pages[page_number])
            rect = RectangleObject((x0, y0, x1, y1))
            new_page.mediabox = rect
            new_page.cropbox = rect
            writer.add_page(new_page)
            return

        if w > max_width and w >= h:
            mid_x = x0 + w / 2
            left = (x0, y0, mid_x, y1)
            right = (mid_x, y0, x1, y1)
            self._split_page(page_number, left, max_width, max_height, reader, writer)
            self._split_page(page_number, right, max_width, max_height, reader, writer)
        else:
            mid_y = y0 + h / 2
            bottom = (x0, y0, x1, mid_y)
            top = (x0, mid_y, x1, y1)
            self._split_page(page_number, top, max_width, max_height, reader, writer)
            self._split_page(page_number, bottom, max_width, max_height, reader, writer)

    def _normalize_rotation(self, page):
        """Return a rotation-normalized page."""
        rotation = page.rotation or 0
        if rotation != 0:
            page.transfer_rotation_to_content()
        return page

    def convert_pdf_to_images(self, path_to_pdf: str | None = None, save_path: str | None = None, dpi: int = 600, padding: int = 0) -> list[str]:
        """Convert PDF pages to PNG images and return a list of saved file paths.

        ``padding`` adds a uniform white border (in pixels) around every page
        image. Tesseract's LSTM line recogniser expects a quiet zone around a
        text block and tends to clip leading/trailing glyphs on tight cropbox
        crops, so pass ``padding=30`` when rendering images destined for OCR.
        The default of 0 leaves existing (non-OCR) callers unchanged.
        """
        target_path = path_to_pdf or self._file_path
        save_dir = save_path or os.path.dirname(target_path) or "."

        # Create directories if they don't exist
        if save_path:
            os.makedirs(save_path, exist_ok=True)

        try:
            pages = convert_from_path(target_path, dpi=dpi, use_cropbox=True, poppler_path=POPPLER_BIN)
        except Exception as e:
            pages = convert_from_path(target_path, dpi=200, use_cropbox=True, poppler_path=POPPLER_BIN)

        base_name = os.path.basename(target_path)
        name_only = os.path.splitext(base_name)[0]

        generated_files = []
        for i, page in enumerate(pages):
            output_file = os.path.join(save_dir, f"{name_only}_{i+1}.png")

            if padding > 0:
                # White border so Tesseract sees a quiet zone around the text.
                page = ImageOps.expand(page, border=padding, fill="white")

            page.save(output_file, format="PNG")
            generated_files.append(output_file)

        return generated_files

    
    def get_page_rotations(self, path_to_pdf: str | None = None) -> list[int]:
        """Return a list of effective rotation angles (0, 90, 180, 270) for each page."""
        target_path = path_to_pdf or self._file_path
        reader = PdfReader(target_path)
        return [int(page.get('/Rotate') or 0) % 360 for page in reader.pages]

    def get_pdf_page_dimensions(self, path_to_pdf: str | None = None) -> list[tuple[float, float]]:
        """Return (width, height) in PDF points for each page, accounting for rotation."""
        target_path = path_to_pdf or self._file_path
        reader = PdfReader(target_path)
        dims = []
        for page in reader.pages:
            w, h = float(page.mediabox.width), float(page.mediabox.height)
            rotation = int(page.get('/Rotate') or 0) % 360
            if rotation in (90, 270):
                w, h = h, w
            dims.append((w, h))
        return dims

    def get_size_of_pdf_pages(self, path_to_pdf: str | None = None) -> list[str | None]:
        """Return a list of page sizes (e.g., 'A4', 'A3') for each page in the PDF."""
        sizes = []
        target_path = path_to_pdf or self._file_path
        reader = PdfReader(target_path)

        for page in reader.pages:
            w, h = float(page.mediabox.width), float(page.mediabox.height)
            sizes.append(self._pdf_size_name(w, h))

        return sizes

    @staticmethod
    def get_size_of_image(image_path: str) -> tuple[int, int]:
        """Return (width, height) of a PNG image by reading its header bytes."""
        with open(image_path, "rb") as f:
            f.read(16)  # Skip PNG signature (8 bytes) + IHDR chunk header (8 bytes)
            width = struct.unpack(">I", f.read(4))[0]
            height = struct.unpack(">I", f.read(4))[0]
        return width, height
    
    @staticmethod
    def normalized_to_pdf_points(
        page_width: float,
        page_height: float,
        nx1: float,
        ny1: float,
        nx2: float,
        ny2: float,
    ) -> tuple[float, float, float, float]:
        """Convert a normalized image-space bbox to PDF points.
        """
        x0 = nx1 * page_width
        x1 = nx2 * page_width
        y0 = (1 - ny2) * page_height
        y1 = (1 - ny1) * page_height
        return x0, y0, x1, y1

    @staticmethod
    def _visual_rect_to_unrotated(
        x0: float, y0: float, x1: float, y1: float,
        mediabox_w: float, mediabox_h: float, rotation: int,
    ) -> tuple[float, float, float, float]:
        """Map a crop rect from rotation-aware visual space to the page's
        unrotated mediabox space.

        """
        rotation = rotation % 360
        if rotation == 0:
            return x0, y0, x1, y1
        if rotation == 90:
            return mediabox_w - y1, x0, mediabox_w - y0, x1
        if rotation == 180:
            return (
                mediabox_w - x1, mediabox_h - y1,
                mediabox_w - x0, mediabox_h - y0,
            )
        if rotation == 270:
            return y0, mediabox_h - x1, y1, mediabox_h - x0
        raise ValueError(f"Unsupported page rotation: {rotation}")

    @staticmethod
    def _mat_mul(m1, m2):
        """Multiply two PDF affine matrices (row-vector convention, m1 applied first)."""
        a1, b1, c1, d1, e1, f1 = m1
        a2, b2, c2, d2, e2, f2 = m2
        return (
            a1 * a2 + b1 * c2,
            a1 * b2 + b1 * d2,
            c1 * a2 + d1 * c2,
            c1 * b2 + d1 * d2,
            e1 * a2 + f1 * c2 + e2,
            e1 * b2 + f1 * d2 + f2,
        )

    def _filter_text_outside(self, page, reader, rect, pad: float = 1.0) -> None:
        """Drop text-showing operators whose origin falls outside ``rect``.

        ``rect`` is (x0, y0, x1, y1) in the page's unrotated user space — the same
        space mediabox/cropbox live in. Graphics and state operators are kept; only
        Tj/TJ/'/" outside the box are removed, so text extraction on the cropped
        page returns only what was visible.
        """
        from pypdf.generic import ContentStream

        raw = page.get("/Contents")
        if raw is None:
            return
        cs = ContentStream(raw.get_object(), reader)

        x0, y0, x1, y1 = rect
        x0 -= pad; y0 -= pad; x1 += pad; y1 += pad

        identity = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
        ctm = identity
        ctm_stack: list = []
        tm = tlm = identity
        leading = 0.0

        show_ops = {b"Tj", b"TJ", b"'", b'"'}
        kept = []

        for operands, op in cs.operations:
            if op == b"q":
                ctm_stack.append(ctm)
            elif op == b"Q":
                if ctm_stack:
                    ctm = ctm_stack.pop()
            elif op == b"cm":
                ctm = self._mat_mul(tuple(float(o) for o in operands), ctm)
            elif op == b"BT":
                tm = tlm = identity
            elif op == b"Tm":
                tm = tlm = tuple(float(o) for o in operands)
            elif op == b"Td":
                tlm = self._mat_mul((1, 0, 0, 1, float(operands[0]), float(operands[1])), tlm)
                tm = tlm
            elif op == b"TD":
                leading = -float(operands[1])
                tlm = self._mat_mul((1, 0, 0, 1, float(operands[0]), float(operands[1])), tlm)
                tm = tlm
            elif op == b"TL":
                leading = float(operands[0])
            elif op == b"T*":
                tlm = self._mat_mul((1, 0, 0, 1, 0, -leading), tlm)
                tm = tlm

            if op in show_ops:
                if op in (b"'", b'"'):          # these move to the next line first
                    tlm = self._mat_mul((1, 0, 0, 1, 0, -leading), tlm)
                    tm = tlm
                trm = self._mat_mul(tm, ctm)    # text -> device
                px, py = trm[4], trm[5]
                if x0 <= px <= x1 and y0 <= py <= y1:
                    kept.append((operands, op))
                # otherwise: dropped
            else:
                kept.append((operands, op))

        cs.operations = kept
        page.replace_contents(cs)

    def cropping_pdf(self, path_to_pdf: str | None = None, save_path: str | None = None,
                    crop_box: list[tuple[int, float, float, float, float]] | None = None,
                    name_of_file: str | None = None, strip_text_outside: bool = True) -> PdfWriter:
        """Build a new PDF where each entry in ``crop_box`` produces one cropped page.

        When ``strip_text_outside`` is True, text outside each crop rectangle is
        removed from the content stream, so the page contains only the visible text.
        """
        if not crop_box:
            return PdfWriter()

        target_path = path_to_pdf or self._file_path
        reader = PdfReader(target_path)
        writer = PdfWriter()

        for page_num, x0, y0, x1, y1 in crop_box:
            source_page = reader.pages[page_num]

            x0, x1 = (x0, x1) if x0 <= x1 else (x1, x0)
            y0, y1 = (y0, y1) if y0 <= y1 else (y1, y0)

            mediabox_w = float(source_page.mediabox.width)
            mediabox_h = float(source_page.mediabox.height)
            rotation = int(source_page.get('/Rotate') or 0) % 360

            ux0, uy0, ux1, uy1 = self._visual_rect_to_unrotated(
                x0, y0, x1, y1, mediabox_w, mediabox_h, rotation,
            )

            ux0 = max(0.0, min(ux0, mediabox_w))
            ux1 = max(0.0, min(ux1, mediabox_w))
            uy0 = max(0.0, min(uy0, mediabox_h))
            uy1 = max(0.0, min(uy1, mediabox_h))

            ux0, ux1 = (ux0, ux1) if ux0 <= ux1 else (ux1, ux0)
            uy0, uy1 = (uy0, uy1) if uy0 <= uy1 else (uy1, uy0)

            new_page = copy.copy(source_page)

            if strip_text_outside:
                self._filter_text_outside(new_page, reader, (ux0, uy0, ux1, uy1))

            rect = RectangleObject((ux0, uy0, ux1, uy1))
            new_page.mediabox = rect
            new_page.cropbox = rect
            writer.add_page(new_page)

        if save_path:
            base_name = os.path.basename(target_path)
            name, ext = os.path.splitext(base_name)
            output_file = os.path.join(save_path, f"{name_of_file or name}{ext}")
            os.makedirs(save_path, exist_ok=True)
            with open(output_file, "wb") as f:
                writer.write(f)

        return writer
    
    def extract_pdf_pages(self, page_index: list[int], save_path: str | None = None, name_of_file: str | None = None) -> PdfWriter:
        reader = PdfReader(self._file_path)
        writer = PdfWriter()
        page_count = len(reader.pages)

        for i in page_index:
            if 0 <= i < page_count:
                writer.add_page(reader.pages[i])
            else:
                print(f"Warning: Page index {i} is out of bounds and was skipped.")

        if save_path:
            os.makedirs(save_path, exist_ok=True)
            base = name_of_file or os.path.splitext(os.path.basename(self._file_path))[0]
            if not base.lower().endswith('.pdf'):
                base += '.pdf'
            with open(os.path.join(save_path, base), "wb") as output_pdf:
                writer.write(output_pdf)

        return writer