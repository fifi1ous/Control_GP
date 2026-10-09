import os
import re

from process_pdf import ProcessPDF
from process_gp import ProcessGP

def extract_text(pdf_path, type="plain"):
    pdf_processor = ProcessPDF(pdf_path)

    return pdf_processor.extract_text(path_to_pdf = pdf_path, type=type)

def convert_pdf_to_images(pdf_path, output_folder):
    pdf_processor = ProcessPDF(pdf_path)

    pages = pdf_processor.convert_pdf_to_images(save_path=output_folder, dpi=600)

    return pages

_NAME_TOKEN = r'((?:st\.?\s*)?\d+(?:/\d+)?)'

# Updated to include "výpočet výměry parc." variations
NAME_RE = re.compile(
    r'(?:'
    r'Označení\s+parcely(?:\s*\(dílu\))?\s*:\s*'   
    r'|Parcela(?:\s+(?:číslo|\(dílu\)))?\s*:\s*'   
    r'|nová\s+parcela\s+'                          
    r'|výpočet\s+výměry\s+parc\.\s*(?:poz\.\s*)?'  # <--- NEW LINE ADDED
    r')' + _NAME_TOKEN,
    re.IGNORECASE,
)

# Updated to make the colon or equals sign [:=] optional
AREA_RE = re.compile(
    r'(?:plocha|výměra)\s*(?:[:=]\s*)?'            # <--- CHANGED THIS LINE
    r'(\d[\d \u00a0]*(?:[.,]\d+)?)'
    r'\s*m\s*[²2]',
    re.IGNORECASE,
)


def _norm_name(raw):
    s = re.sub(r'\s+', '', raw.strip())            
    m = re.match(r'(?i)^st\.?(\d+(?:/\d+)?)$', s)
    if m:
        return 'st. ' + m.group(1)                 
    return s


def _to_float(s):
    return float(s.replace('\u00a0', '').replace(' ', '').replace(',', '.'))


def extract_vymery_prot(path):
    """Return ["name area", ...], one string per parcel."""
    text = extract_text(path)
    matches = list(NAME_RE.finditer(text))
    results = []
    for i, m in enumerate(matches):
        block_start = m.end()
        block_end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        a = AREA_RE.search(text[block_start:block_end])
        name = _norm_name(m.group(1))
        area = _to_float(a.group(1)) if a else None
        results.append(f"{name} {area}")
    return results

def extract_vymery_gp(pdf_path):
    gp_processor = ProcessGP(pdf_path)
    return gp_processor.extract_vykaz()

def extract_vymery_vypocet(pdf_path):
    gp_processor = ProcessGP(pdf_path)
    vymery = gp_processor.extract_vypocet_vymer()
    
    vymery_processed = []
    seen_identifiers_for_2 = set()
    was_original = False
    
    for v in vymery:
        if len(v) > 0 and v[0] == 1:
            was_original = True
            
        if was_original:
            if len(v) > 1 and v[0] == 2:
                identifier = v[1]
                if identifier not in seen_identifiers_for_2:
                    vymery_processed.append(v)
                    seen_identifiers_for_2.add(identifier)
            else:
                vymery_processed.append(v)
                
    return vymery_processed
