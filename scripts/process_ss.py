import os
import re

from process_pdf import ProcessPDF

def extract_text(pdf_path, type="plain"):
    pdf_processor = ProcessPDF(pdf_path)

    return pdf_processor.extract_text(path_to_pdf = pdf_path, type=type)

def convert_pdf_to_images(pdf_path, output_folder):
    pdf_processor = ProcessPDF(pdf_path)

    pages = pdf_processor.convert_pdf_to_images(save_path=output_folder, dpi=600, padding=50)

    return pages

def get_coordinates_from_prot(pdf_path):
    pdf_processor = ProcessPDF(pdf_path)
    text = pdf_processor.extract_text(path_to_pdf=pdf_path)

    # Match only the heading line; no DOTALL needed here.
    heading = re.compile(
        r"(?im)^\s*Seznam souřadnic(?![^\n]*(?:rušen|dan))[^\n]*"
    )
    matches = list(heading.finditer(text))
    if not matches:
        return ""

    results = []
    for i, m in enumerate(matches):
        start = m.end()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        section = text[start:end]

        ss = []
        maximum_free_lines = 5
        while not ss and maximum_free_lines <= 20:
            ss = parse_ss(section, maximum_free_lines)
            maximum_free_lines += 1
        if ss:
            results.extend(ss)

    return results

def parse_ss(text, maximum_free_lines=10):
    """
    Collects the raw data lines of a point table.

    A line is kept only if its first token is a number. Decoration/header
    lines containing any of:  číslo  -  =  _  *  k.ú.  are skipped outright.
    Returns a flat list of the kept lines as strings: ["line1", "line2", ...]

    The free-lines counter is preserved so the escalating loop in
    get_coordinates_from_prot still detects the end of the table.
    """
    result_list = []
    free_lines_count = 0

    SKIP_TOKENS = ("číslo", "k.ú.")          # word/label markers (case-insensitive)
    SKIP_CHARS = "-=_*"                       # decoration / separator characters

    for line in text.strip().split("\n"):
        cleaned_line = line.replace("|", " ").strip()
        lower = cleaned_line.lower()

        # Skip blank lines and any decoration/header line, WITHOUT counting it
        if (
            not cleaned_line
            or any(tok in lower for tok in SKIP_TOKENS)
            or any(ch in cleaned_line for ch in SKIP_CHARS)
        ):
            continue

        # Stop once we've seen too many consecutive non-data lines
        if free_lines_count >= maximum_free_lines:
            break

        tokens = cleaned_line.split()
        if not tokens:
            free_lines_count += 1
            continue

        if not tokens[0].isdigit():
            free_lines_count += 1
            continue
        else:
            free_lines_count = 0

        result_list.append(cleaned_line)

    return result_list
