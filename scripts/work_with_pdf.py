import os
import re

from process_pdf import ProcessPDF
from ultralytics import YOLO


def _annotation_filename(page_index: int) -> str:
    """Return the annotation file name for a 0-based page index."""
    return f"page_{page_index + 1}.txt"


def _page_index_from_annotation_filename(filename: str) -> int:
    """Parse a 0-based page index from an annotation file name produced by
    :func:`_annotation_filename`."""
    stem = os.path.splitext(os.path.basename(filename))[0]
    return int(stem.split('_', 1)[1]) - 1


def segment_geometric_plan(pdf_path, path_GP_folder, model, path_to_annotations_folder):

    pdf_processor = ProcessPDF(pdf_path)

    pages = pdf_processor.convert_pdf_to_images(save_path=path_GP_folder)

    os.makedirs(path_to_annotations_folder, exist_ok=True)

    for i, page_img_path in enumerate(pages):
        results = model(page_img_path)

        annot_file = os.path.join(path_to_annotations_folder, _annotation_filename(i))
        with open(annot_file, "w") as f:
            for result in results:
                # YOLO stores the source image shape as (height, width)
                height_image, width_image = result.orig_shape
                for box in result.boxes:
                    # Extract coordinates from YOLO
                    x1, y1, x2, y2 = [v.item() for v in box.xyxy[0]]
                    class_id = int(box.cls[0].item())

                    # Convert to PDF points
                    nx1, ny1, nx2, ny2 = convert_image_coordinates_to_normalized(
                        x1, y1, x2, y2, (width_image, height_image)
                    )

                    # Save annotation
                    f.write(f"{class_id} {nx1} {ny1} {nx2} {ny2}\n")

def convert_image_coordinates_to_normalized(x1, y1, x2, y2, size_of_image):
    width_image, height_image = size_of_image

    # Divide the absolute pixel coordinate by the total image dimensions
    nx1 = x1 / width_image
    ny1 = y1 / height_image
    nx2 = x2 / width_image
    ny2 = y2 / height_image

    # We use 6 decimal places because percentages need high precision
    return round(nx1, 6), round(ny1, 6), round(nx2, 6), round(ny2, 6)

# Divide PDF into separate pages based on annotations as pdf and images
def divide_pdf_into_separate_pages(pdf_path, path_to_annotations_folder, output_folder, class_id, name_of_file=None):
    # initialize PDF processor
    pdf_processor = ProcessPDF(pdf_path)

    # page dimensions in PDF points (rotation-aware), indexed 0-based
    page_dims = pdf_processor.get_pdf_page_dimensions()

    # get files in annotations folder
    annotation_files = [f for f in os.listdir(path_to_annotations_folder) if f.endswith('.txt')]

    # (page_index, x0, y0, x1, y1) tuples ready for ProcessPDF.cropping_pdf
    pages_with_class = []

    for annot_file in annotation_files:
        page_index = _page_index_from_annotation_filename(annot_file)
        page_width, page_height = page_dims[page_index]

        with open(os.path.join(path_to_annotations_folder, annot_file), 'r') as f:
            lines = f.readlines()
            for line in lines:
                parts = line.strip().split()
                class_id_in_file, nx1, ny1, nx2, ny2 = parts
                if class_id_in_file != str(class_id):
                    continue

                # convert normalized image-space bbox to PDF points
                x0, y0, x1, y1 = ProcessPDF.normalized_to_pdf_points(
                    page_width, page_height,
                    float(nx1), float(ny1), float(nx2), float(ny2),
                )

                pages_with_class.append((page_index, x0, y0, x1, y1))

    # crop PDF based on collected pages_with_class
    pdf_processor.cropping_pdf(save_path=output_folder, crop_box=pages_with_class, name_of_file=name_of_file)

def extract_text(pdf_path, type="plain"):
    pdf_processor = ProcessPDF(pdf_path)

    return pdf_processor.extract_text_processor(path_to_pdf = pdf_path, type=type)

def convert_pdf_to_images(pdf_path, output_folder):
    pdf_processor = ProcessPDF(pdf_path)

    pages = pdf_processor.convert_pdf_to_images(save_path=output_folder, dpi=600)

    return pages

def classify_pdf(folder_path, path_to_gnss, save_path, path_to_model):
    # create a list of all files in the folder and subfolders
    all_files = []
    for root, dirs, files in os.walk(folder_path):
        for file in files:
            all_files.append(os.path.join(root, file))

    overeni_file_path, file_names = get_signed_pdf(all_files)

    info_about_files = []

    for file_path in all_files:
        info_about_file = []
        name = os.path.basename(file_path)
        if name in file_names:
            if name.endswith(".pdf"):
                pdf_processor = ProcessPDF(file_path)
                if "oprav" in name.lower():
                    pdf_metadata = pdf_processor.get_pdf_info()

                    effective_version = pdf_metadata.get('effective_version')
                    is_pdfa = pdf_metadata.get('pdfa', {}).get('is_pdfa')
                    part = pdf_metadata.get('pdfa', {}).get('part')
                    description = pdf_metadata.get('pdfa', {}).get('description')
                    signature_count = pdf_metadata.get('signatures', {}).get('count')
                    capability = (True if pdf_metadata.get('signatures', {}).get('capability') == 'Povoleno' else False)
                    page_sizes = pdf_metadata.get('page_sizes', [])
                    number_of_pages = len(page_sizes)

                    info_about_file.extend(["oprav", name, file_path, effective_version, is_pdfa, part, description, signature_count, capability, number_of_pages, page_sizes])
                    if info_about_file:
                        info_about_files.append(info_about_file)
                    continue

                classes, gnss_indices = classification(file_path, save_path, path_to_model=path_to_model)
                
                if gnss_indices:
                    pdf_processor.extract_pdf_pages(page_index=gnss_indices, save_path=path_to_gnss, name_of_file="gnss.pdf")
                    info_about_files.append(["gnss"])

                pdf_metadata = pdf_processor.get_pdf_info()

                effective_version = pdf_metadata.get('effective_version')
                is_pdfa = pdf_metadata.get('pdfa', {}).get('is_pdfa')
                part = pdf_metadata.get('pdfa', {}).get('part')
                description = pdf_metadata.get('pdfa', {}).get('description')
                signature_count = pdf_metadata.get('signatures', {}).get('count')
                capability = (True if pdf_metadata.get('signatures', {}).get('capability') == 'Povoleno' else False)
                page_sizes = pdf_metadata.get('page_sizes', [])
                number_of_pages = len(page_sizes)

                info_about_file.extend([classes, name, file_path, effective_version, is_pdfa, part, description, signature_count, capability, number_of_pages, page_sizes])

            elif name.endswith(".vfk") or (name.endswith(".txt") and "overeni" not in name.lower()):
                info_about_file.extend(["vfk", name, file_path])
            
            if info_about_file:
                info_about_files.append(info_about_file)
            continue

        if name.endswith(".pdf"):
            if "protokolovereni" in name.lower():
                info_about_file.extend(["overeni", name, file_path])
            else:
                classes = classification(file_path, save_path, path_to_model=path_to_model)

                if (classes[0] == "gp" and re.match(r'^\d{6}_GP_\d{5}', name)) or classes[0] == "zadost":
                    pdf_processor = ProcessPDF(file_path)
                    pdf_metadata = pdf_processor.get_pdf_info()

                    effective_version = pdf_metadata.get('effective_version')
                    is_pdfa = pdf_metadata.get('pdfa', {}).get('is_pdfa')
                    part = pdf_metadata.get('pdfa', {}).get('part')
                    description = pdf_metadata.get('pdfa', {}).get('description')
                    signature_count = pdf_metadata.get('signatures', {}).get('count')
                    capability = (True if pdf_metadata.get('signatures', {}).get('capability') == 'Povoleno' else False)
                    page_sizes = pdf_metadata.get('page_sizes', [])
                    number_of_pages = len(page_sizes)

                    info_about_file.extend([classes[0], name, file_path, effective_version, is_pdfa, part, description, signature_count, capability, number_of_pages, page_sizes])
                
                else:
                    info_about_file.extend([classes[0], name, file_path])

        else:
            if os.path.basename(overeni_file_path) == name:
                info_about_file.extend(["AZI1",os.path.basename(overeni_file_path), overeni_file_path])
            elif os.path.basename(overeni_file_path + ".p7s") == name:
                info_about_file.extend(["AZI2",os.path.basename(overeni_file_path + ".p7s"), overeni_file_path + ".p7s"])
            elif os.path.basename(overeni_file_path + ".p7s.tsr") == name:
                info_about_file.extend(["AZI3",os.path.basename(overeni_file_path + ".p7s.tsr"), overeni_file_path + ".p7s.tsr"])
            else:
                info_about_file.extend([name, file_path])
            
            
        if info_about_file:
            info_about_files.append(info_about_file)


    return info_about_files

def get_signed_pdf(all_files):
    # find the path to the file that has overeni in its name and end with .txt
    overeni_file_path = None
    for file in all_files:
        if "overeni" in file.lower() and file.endswith(".txt"):
            overeni_file_path = file
            break

    file_names = []
        
    with open(overeni_file_path, 'r') as soubor:
        start_file_colecting = False
        
        for row in soubor:
            row = row.strip()
            
            if row == "----":
                start_file_colecting = True
                continue
            
            if start_file_colecting and row:
                name = row.split(';')[0]
                file_names.append(name)
    
    return overeni_file_path, file_names
                

def classification(pdf_path, save_path, path_to_model):
    pdf_processor = ProcessPDF(pdf_path)

    pages = pdf_processor.convert_pdf_to_images(save_path=save_path)

    classifications = []
    model = YOLO(path_to_model)

    for page_img_path in pages:
        results = model(page_img_path)
        top_class_id = results[0].probs.top1
        classifications.append(results[0].names[top_class_id])

    if not classifications:
        return None, []

    gnss_indices = [i for i, cls in enumerate(classifications) if cls == 'gnss']

    has_gnss = len(gnss_indices) > 0
    has_prot = 'prot' in classifications
    has_zap = 'zap' in classifications


    if has_gnss or has_prot or has_zap:
        prot_count = classifications.count('prot')
        zap_count = classifications.count('zap')
        
        main_class = 'prot' if prot_count >= zap_count else 'zap'
        return main_class, gnss_indices


    scores = {}
    for cls in classifications:
        if cls not in scores:
            scores[cls] = 0
            

        if cls == 'nacrt':
            scores[cls] += 0.75 
        else:
            scores[cls] += 1.0
            
    main_class = max(scores, key=scores.get)

    return main_class, gnss_indices


def get_info_about_pdf(path_to_pdf):
    pdf_processor = ProcessPDF(path_to_pdf)
    pdf_metadata = pdf_processor.get_pdf_info()

    effective_version = pdf_metadata.get('effective_version')
    is_pdfa = pdf_metadata.get('pdfa', {}).get('is_pdfa')
    part = pdf_metadata.get('pdfa', {}).get('part')
    description = pdf_metadata.get('pdfa', {}).get('description')
    signature_count = pdf_metadata.get('signatures', {}).get('count')
    capability = (True if pdf_metadata.get('signatures', {}).get('capability') == 'Povoleno' else False)
    page_sizes = pdf_metadata.get('page_sizes', [])
    number_of_pages = len(page_sizes)


    return [effective_version, is_pdfa, part, description, signature_count, capability, number_of_pages, page_sizes]
