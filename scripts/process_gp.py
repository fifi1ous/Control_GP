import re
import os

from pypdf import PdfReader

from process_pdf import ProcessPDF


# ── "Výkaz dosavadního a nového stavu" parser constants ──
_VYKAZ_ID_RE = re.compile(r"(?:st\.?\s*)?\d+(?:/\d+)?$")
_VYKAZ_DIGITS = re.compile(r"\d+")
# Footnote markers attached to a parcel, e.g. "*1)", "1)*", "2)" — stripped
# before parsing so they don't break id/výměra detection.
_VYKAZ_NOTE_RE = re.compile(r"\*?\d+\)\*?")
# Any of these words marks a header / unit fragment (used to exclude header rows
# when measuring the column grid).
_VYKAZ_HEADER_WORDS = (
    "Označení", "parc.", "Výměra", "Druh", "Typ", "Způs.", "výměr", "Díl",
    "Porovnání", "pozemku", "Způsob", "využití", "VÝKAZ", "stav", "určení",
    "číslem", "katastru", "nemovitostí", "evidenc", "vlastnictví", "dílu",
)
# Recurring header words whose character offsets define the column grid.
_VYKAZ_ANCHORS = (
    "Označení", "parc.", "Výměra", "Druh", "Typ", "Způs.", "výměr",
    "Díl", "Porovnání",
)



# ── "Výpočet výměr parcel (dílů)" parser constants ──
# The 12 numbered column headers ("1 2 … 12") printed on every page of the form;
# their character offsets in layout text give the per-page column grid.
_VYP_COLNUM = [str(i) for i in range(1, 13)]
_VYP_KOD_RE = re.compile(r"\d{1,2}$")          # způsob určení výměry: a 1-2 digit code
_VYP_SKIP_WORDS = ("č.zakázky", "k.ú.", "List katastrální")


class ProcessGP(ProcessPDF):
    """Geometric-plan (geometrický plán) specific PDF processing.

    Extends :class:`ProcessPDF` with parsers that are specific to geometric
    plan documents — the "Výkaz dosavadního a nového stavu" table and the
    "Seznam souřadnic" point tables.
    """

    # ──────────────────────────────────────────────────────────────────────
    #  Výkaz dosavadního a nového stavu  →  structured parcel data
    # ──────────────────────────────────────────────────────────────────────
    @staticmethod
    def _vykaz_is_id(token: str) -> bool:
        """True if ``token`` looks like a parcel designation (e.g. '315/2', 'st.157')."""
        return bool(_VYKAZ_ID_RE.fullmatch(token.replace(" ", "")))

    @staticmethod
    def _vykaz_area(text: str) -> int:
        """Fold the ha / a / m² figures found in ``text`` into a single area in m²."""
        nums = _VYKAZ_DIGITS.findall(text)
        if len(nums) >= 3:
            return int(nums[0]) * 10000 + int(nums[1]) * 100 + int(nums[2])
        if len(nums) == 2:
            return int(nums[0]) * 100 + int(nums[1])
        if len(nums) == 1:
            return int(nums[0])
        return 0

    @classmethod
    def _vykaz_parse_stav(cls, text: str) -> tuple[str, int, str] | None:
        """Parse one stav cell ('id  výměra…  druh pozemku') -> (id, area_m2, druh).

        The id is the first token; the following run of integers is the výměra
        (ha/a/m²); the remaining non-numeric tokens form the druh pozemku.
        Returns None when the cell does not start with a parcel id. Collecting
        all integers as the area keeps single-space-separated sub-columns
        (e.g. '6 27') together.
        """
        toks = [t for t in text.split() if not _VYKAZ_NOTE_RE.fullmatch(t)]
        if not toks or not cls._vykaz_is_id(toks[0]):
            return None
        ident, i, nums = toks[0], 1, []
        while i < len(toks) and toks[i].isdigit():
            nums.append(toks[i])
            i += 1
        type_words = [t for t in toks[i:] if not t.isdigit()]
        return ident, cls._vykaz_area(" ".join(nums)), " ".join(type_words)

    @staticmethod
    def _vykaz_header_offsets(lines: list[str]) -> dict[str, list[int]]:
        """Character offsets of each recurring header word, sorted left→right."""
        pos: dict[str, list[int]] = {}
        for ln in lines:
            for lab in _VYKAZ_ANCHORS:
                s = 0
                while (i := ln.find(lab, s)) >= 0:
                    pos.setdefault(lab, []).append(i)
                    s = i + 1
        return {k: sorted(v) for k, v in pos.items()}

    @classmethod
    def _vykaz_build_columns(cls, lines: list[str], pos: dict[str, list[int]]) -> dict[str, int] | None:
        """Coarse column boundaries for one page: dosavadní|nový split, nový
        type|Typ stavby, Způs. and Porovnání.

        Each header anchor is snapped into a real ≥2-wide whitespace gap measured
        over data rows only, so a boundary never lands inside a cell. Returns None
        when the page carries no recognisable header (e.g. a continuation page).
        """
        if len(pos.get("Výměra", [])) < 2 or len(pos.get("Druh", [])) < 2:
            return None

        width = max((len(l) for l in lines), default=0)
        body = [l for l in lines if l.strip() and re.search(r"\d", l)
                and not any(h in l for h in _VYKAZ_HEADER_WORDS)]
        if not body:
            body = [l for l in lines if l.strip()]

        blank = [c for c in range(width) if all(c >= len(r) or r[c] == " " for r in body)]
        runs, i = [], 0
        while i < len(blank):
            j = i
            while j + 1 < len(blank) and blank[j + 1] == blank[j] + 1:
                j += 1
            if blank[j] - blank[i] >= 1:        # gap run spanning ≥2 columns
                runs.append(blank[i])
            i = j + 1

        def snap(anchor: int) -> int:
            left = [c for c in runs if c <= anchor]
            return left[-1] if left else 0

        druh, vym = pos["Druh"], pos["Výměra"]
        novid = next((n for n in sorted(pos.get("Označení", []) + pos.get("parc.", []))
                      if n > druh[0]), vym[1])
        cut = (pos.get("Díl") or pos.get("Porovnání") or [width])[0]
        typ = (pos.get("Typ") or pos.get("Způs.") or [cut])[0]
        zps = (pos.get("Způs.") or pos.get("výměr") or [cut])[0]
        return {"split": snap(novid), "typ": snap(typ), "zps": snap(zps), "cut": snap(cut)}

    def extract_vykaz(self, path_to_pdf: str | None = None) -> list[list]:
        """Parse a 'Výkaz dosavadního a nového stavu údajů katastru nemovitostí'.

        Universal across any number of rows and pages; ignores the 'Porovnání se
        stavem evidence právních vztahů' block and the Typ stavby column, and
        drops header / unit fragments ('ha', 'm²', 'Způsob využití', …).

        Returns a flat list of rows in document order:
            [1, parc_id, area_m2, druh]              – dosavadní stav
            [2, parc_id, area_m2, druh, zpusob]      – nový stav (zpusob ∈ {'0','1','2'}/None)
            [3, area_dosavadni_m2, area_novy_m2]     – součtový (totals) řádek
        """
        target_path = path_to_pdf or self._file_path
        if not os.path.isfile(target_path):
            raise FileNotFoundError(f"PDF nenalezen: {target_path}")

        # Rotated pages yield empty layout text, so bake any /Rotate into the
        # content first (a fresh reader per call keeps this non-destructive).
        reader = PdfReader(target_path)
        pages = [self._normalize_rotation(page).extract_text(extraction_mode="layout")
                 for page in reader.pages]

        out: list[list] = []
        cols: dict[str, int] | None = None
        for page_text in pages:
            lines = page_text.splitlines()
            pos = self._vykaz_header_offsets(lines)
            new_cols = self._vykaz_build_columns(lines, pos)
            if new_cols:                # reuse previous page's grid on header-less pages
                cols = new_cols
            if not cols:
                continue

            seen_data = False
            for ln in lines:
                if not ln.strip() or "(" in ln:   # blanks + "( 17 04 )" díl subtotals
                    continue
                head = ln[:cols["cut"]]                       # everything left of Porovnání
                dos = ln[:cols["split"]]
                nov = ln[cols["split"]:cols["typ"]]           # nový id+area+type, no Typ stavby
                # způsob určení výměr = the right-most lone 0/1/2 before Porovnání
                zt = [t for t in ln[cols["typ"]:cols["cut"]].split() if t in ("0", "1", "2")]
                n_z = zt[-1] if zt else None

                if not any(ch.isalpha() for ch in head):      # pure-numeric line
                    if seen_data:
                        da = self._vykaz_area(dos)
                        na = self._vykaz_area(ln[cols["split"]:cols["cut"]])
                        if da and na:
                            out.append([3, da, na])           # součtový řádek
                    continue                                  # else: 'ha m²' header markers

                d = self._vykaz_parse_stav(dos)
                n = self._vykaz_parse_stav(nov)
                if (d and d[1]) or (n and n[1]):
                    seen_data = True
                    if d and d[1]:
                        out.append([1, d[0], d[1], d[2]])
                    if n and n[1]:
                        out.append([2, n[0], n[1], n[2], n_z])
        return out

    # ──────────────────────────────────────────────────────────────────────
    #  Výpočet výměr parcel (dílů)  →  structured parcel data
    # ──────────────────────────────────────────────────────────────────────
    @staticmethod
    def _vyp_col_offsets(line: str) -> list[int] | None:
        """Character offsets of the '1 2 … 12' numbered header row, else None.

        This row is printed on every page of the form and defines the column
        grid; offsets drift a few characters between documents, so they are
        re-measured per page rather than hard-coded.
        """
        if line.split()[:12] != _VYP_COLNUM:
            return None
        offs, s = [], 0
        for n in _VYP_COLNUM:
            i = line.find(n, s)
            offs.append(i)
            s = i + len(n)
        return offs

    @staticmethod
    def _vyp_columns(line: str, offs: list[int]) -> list[list[str]]:
        """Bucket each whitespace-separated token into the nearest of 12 columns."""
        cols: list[list[str]] = [[] for _ in range(12)]
        for m in re.finditer(r"\S+", line):
            c = min(range(12), key=lambda k: abs(m.start() - offs[k]))
            cols[c].append(m.group())
        return cols

    def extract_vypocet_vymer(self, path_to_pdf: str | None = None) -> list[list]:
        """Parse a 'Výpočet výměr parcel (dílů)' sheet (the *_vymery.pdf form).

        Universal across any number of rows and pages. Returns a flat list of
        records in document order, tagged by a leading id:

            [1, parc_id, area_m2]                  – Dané parcely nebo skupiny
                                                     (col 2 parcela, col 3 výměra)
            [2, parc_id, kod, konecna_m2]          – Počítané výměry (col 5 parcelní,
                                                     col 6 kód způs. určení výměry,
                                                     col 12 konečná výměra)
            [3, area_dosavadni_m2, area_novy_m2]   – dosavadní / nový stav

        On a "Vyrovnání dílů do skupiny" sheet each díl computation is a real
        row in Počítané výměry, so a parcel yields several [2, …] records; the
        per-parcel result is the dané row ([1, …]) for its group.
        """
        target_path = path_to_pdf or self._file_path
        if not os.path.isfile(target_path):
            raise FileNotFoundError(f"PDF nenalezen: {target_path}")

        # Bake any /Rotate into the content first so layout text is non-empty.
        reader = PdfReader(target_path)
        pages = [self._normalize_rotation(page).extract_text(extraction_mode="layout")
                 for page in reader.pages]

        out: list[list] = []
        souhrn: dict[str, int] = {}
        offs: list[int] | None = None              # reuse previous grid on header-less pages

        for page_text in pages:
            lines = page_text.splitlines()
            for ln in lines:
                o = self._vyp_col_offsets(ln)
                if o:
                    offs = o
            if not offs:
                continue

            for ln in lines:
                if not ln.strip() or self._vyp_col_offsets(ln):
                    continue
                if any(w in ln for w in _VYP_SKIP_WORDS):
                    continue

                cols = self._vyp_columns(ln, offs)

                # totals block (dosavadní / nový / rozdíl): value lives in col 3
                low = ln.lower()
                key = next((k for k in ("dosavadní", "nový", "rozdíl") if k in low), None)
                if key:
                    souhrn[key] = self._vykaz_area(" ".join(cols[2]))
                    continue

                # Dané parcely nebo skupiny  (col 2 + col 3)
                p2 = next((t for t in cols[1] if self._vykaz_is_id(t)), None)
                if p2:
                    out.append([1, p2, self._vykaz_area(" ".join(cols[2]))])

                # Počítané výměry  (col 5 + col 6 + col 12)
                p5 = next((t for t in cols[4] if self._vykaz_is_id(t)), None)
                konecna = self._vykaz_area(" ".join(cols[11]))
                kod = next((t for t in cols[5] if _VYP_KOD_RE.fullmatch(t)), None)
                if p5 and konecna:
                    out.append([2, p5, kod, konecna])

        if souhrn:
            out.append([3, souhrn.get("dosavadní"), souhrn.get("nový")])
        return out