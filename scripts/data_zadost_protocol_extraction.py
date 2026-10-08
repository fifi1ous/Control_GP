import re
from pypdf import PdfReader

class DataZadostProtocolExtraction:
    '''
    Class for extracting data from "Zadost.pdf" file and ProtokolOvereni.pdf.
    Attributes to extract from "Zadost.pdf":
    - kur: Name of the cadastral office
    - kp: Name of the cadastral workplace
    - cp: Number of the plan
    - kuz: Cadastral area
    - cr: Number of the process
    - jp: Name of the author
    - azi: Number of AZI

    Attributes to extract from "ProtokolOvereni.pdf" for header:
    - jp: Name of the author
    - cj: Number of the proceeding

    Methods:
    - get_zadost_attributes(path_to_pdf: str) -> dict: Extracts the specified attributes from "Zadost.pdf".
    - get_protokol_attributes_for_header(path_to_pdf: str) -> dict: Extracts the specified attributes from "ProtokolOvereni.pdf" for header.
    '''

    # Regular expressions for extracting data from text
    PATTERNS_ZADOST = {
    "kur": r"Katastrálnímu úřadu pro\s+(.+?)\s*(?=\n|$)",
    "kp" : r"Katastrální pracoviště\s+(.+?)\s*(?=\n|$)",
    "cp" : r"Číslo plánu\s+([\d\-\/]+)",
    "kuz": r"Katastrální území\s+(.+?)\s*(?=\n|$)",
    "cr" : r"Číslo řízení PM 2\)\s+(.+?)\s*(?=\n|$)",
    "jp" : r"datum narození\s*\n\s*(\w+\s+\w+)",
    "azi": r"zeměměřických inženýrů\s*\n\s*(.+)"
    }
    # Additional pattern for AZI if not found in the main patterns
    AZI =  r"úředním oprávněním\s*\n\s*(.+)"

    PATTERNS_ZADOST_HEADER = {
    "jp" : r",CN=([\s\S]+?),\s*C=CZ",
    "cj" : r"Číslo jednací:\s*([\w/-]+)"
    }

    @staticmethod
    def get_zadost_attributes(path_to_pdf: str) -> dict:
        '''
        Method to Extracts specified attributes from the given zadost.pdf.
        '''
        # Dictionary to store results
        results = {}

        # Extract text from the first page of the PDF
        reader = PdfReader(path_to_pdf)
        text = reader.pages[0].extract_text() or ""

        # If text extraction was successful, use regex to find attributes
        if text:

            # Iterate over each pattern and search in the extracted text
            for key, pattern in DataZadostProtocolExtraction.PATTERNS_ZADOST.items():

                # Search for the pattern in the text
                match = re.search(pattern, text)

                # If a match is found, store it in the results dictionary
                if match:

                    # Extracted value is in the first capturing group
                    results[key] = match.group(1).strip()

            # Special handling for 'azi' if not found
            if "azi" not in results.keys():

                # Search for AZI using the alternative pattern
                match = re.search(DataZadostProtocolExtraction.AZI, text)

                # If a match is found, store it in the results dictionary
                if match:
                    results['azi'] = match.group(1).strip().split(' ')[-1]

        return results

    @staticmethod
    def get_protokol_attributes_for_header(path_to_pdf: str) -> dict:
        '''
        Method to Extracts specified attributes from the given ProtokolOvereni.pdf.
        '''
        # Dictionary to store results
        results = {}

        # String to store text
        text = ""

        # Extract text from the pages of the PDF
        reader = PdfReader(path_to_pdf)

        # Iterate over each page
        for page in reader.pages:
            # Extract the text from page and add it to current text
            text += (page.extract_text() or "") + "\n"

        # Iterate over each pattern and search in the extracted text
        for key, pattern in DataZadostProtocolExtraction.PATTERNS_ZADOST_HEADER.items():

            # Search for the pattern in the text
            match = re.search(pattern, text)

            # Get the text
            extracted_text = match.group(1).strip()

            # If key 'Jmnéno Příjmení' check if it has title
            if key == "jp":

                # Split the text by ' '
                words = extracted_text.split()

                # Take the last two words safely and joins them
                extracted_text = " ".join(words[-2:])

            # Add the found text to the dictionary
            results[key] = extracted_text

        return results
