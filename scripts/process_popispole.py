import os
import re

import requests

from process_pdf import ProcessPDF
from data_zadost_protocol_extraction import DataZadostProtocolExtraction
from manage_database import ManageDatabase

def extract_text(pdf_path, type="plain"):
    pdf_processor = ProcessPDF(pdf_path)

    return pdf_processor.extract_text_processor(path_to_pdf = pdf_path, type=type)

def convert_pdf_to_images(pdf_path, output_folder):
    pdf_processor = ProcessPDF(pdf_path)

    pages = pdf_processor.convert_pdf_to_images(save_path=output_folder, dpi=600)

    return pages

def zadost_data(pdf_path):
    results = DataZadostProtocolExtraction.get_zadost_attributes(pdf_path)

    return results

def zadost_jp(pdf_path):
    results = DataZadostProtocolExtraction.get_zadost_attributes(pdf_path)

    return results["jp"] if "jp" in results.keys() else ""

def get_get_azi_number(prijmeni_jmeno):
    parts = prijmeni_jmeno.split()
    if len(parts) < 2:
        return ""
    results = ManageDatabase.get_surveyor_by_name(parts[1], parts[0])

    if results is None:
        return ""
    else:
        return f"{results['AziNumber']} {results['Authorization']}"


def over_zememericskou_cinnost(vstupni_jmeno):
    """Otočí jméno z 'Příjmení Jméno' na 'Jméno Příjmení' a ověří v ARES API v2,

    zda osoba provozuje činnost 'Výkon zeměměřických činností'.
    """
    # Rychlé otočení formátu "Příjmení Jméno" na "Jméno Příjmení"
    casti = vstupni_jmeno.strip().split()
    if len(casti) >= 2:
        # Vezme poslední prvek jako Jméno a zbytek spojí jako Příjmení (řeší i víceslovná příjmení)
        jmeno_osoby = f"{casti[-1]} {' '.join(casti[:-1])}"
    else:
        jmeno_osoby = vstupni_jmeno

    print(f"Původní vstup: '{vstupni_jmeno}' -> Hledaný formát: '{jmeno_osoby}'")

    base_url = "https://ares.gov.cz/ekonomicke-subjekty-v-be/rest"

    # 1. Krok: Vyhledání subjektu podle upraveného jména
    search_url = f"{base_url}/ekonomicke-subjekty/vyhledat"
    payload = {"obchodniJmeno": jmeno_osoby, "start": 0, "pocet": 20}
    response = requests.post(search_url, json=payload, timeout=10)

    try:
        if response.status_code != 200:
            print(f"Chyba při vyhledávání v ARES: {response.status_code}")
            return False

        data = response.json()
        subjekty = data.get("ekonomickeSubjekty", [])

        if not subjekty:
            print(f"Osoba '{jmeno_osoby}' nebyla v registru ARES nalezena.")
            return False

        # Projdeme nalezené subjekty
        for subjekt in subjekty:
            ico = subjekt.get("ico")
            obchodni_jmeno = subjekt.get("obchodniJmeno")

            # 2. Krok: Dotaz na detail subjektu ze zdroje RŽP (Registr živnostenského podnikání)
            rzp_url = f"{base_url}/ekonomicke-subjekty-rzp/{ico}"
            rzp_response = requests.get(rzp_url, timeout=10)

            if rzp_response.status_code != 200:
                continue

            rzp_data = rzp_response.json()

            # Podle dokumentace: EkonomickySubjektRzp -> zaznamy (ZaznamRzp)
            zaznamy = rzp_data.get("zaznamy", [])
            for zaznam in zaznamy:
                # ZaznamRzp -> zivnosti (Zivnost)
                zivnosti = zaznam.get("zivnosti", [])
                for zivnost in zivnosti:
                    # ZivnostZaklad -> predmetPodnikani (Předmět podnikání živnosti)
                    predmet = zivnost.get("predmetPodnikani", "")

                    if "zeměměřických" in predmet.lower():
                        print(
                            f"[SHODA] Osoba: {obchodni_jmeno} (IČO: {ico}) má předmět podnikání: '{predmet}'"
                        )
                        return True

                    # Pro jistotu kontrola i specifických oborů činností (Zivnost -> oboryCinnosti -> oborNazev)
                    obory = zivnost.get("oboryCinnosti", [])
                    for obor in obory:
                        obor_nazev = obor.get("oborNazev", "")
                        if "zeměměřických" in obor_nazev.lower():
                            print(
                                f"[SHODA] Osoba: {obchodni_jmeno} (IČO: {ico}) má obor činnosti: '{obor_nazev}'"
                            )
                            return True

        print(
            f"Osoba '{jmeno_osoby}' byla nalezena, ale nemá zapsanou zeměměřickou činnost."
        )
        return False

    except requests.exceptions.RequestException as e:
        print(f"Chyba sítě/API: {e}")
        return False


