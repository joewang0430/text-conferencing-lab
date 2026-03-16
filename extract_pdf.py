import sys
from pdfminer.high_level import extract_text

def extract_pdf(pdf_path, output_path):
    try:
        text = extract_text(pdf_path)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(text)
        print(f"Successfully extracted text from {pdf_path} to {output_path}")
    except Exception as e:
        print(f"Error extracting text from {pdf_path}: {e}")

if __name__ == "__main__":
    extract_pdf("docs/TextConferencingLab.pdf", "docs/TextConferencingLab.txt")
    extract_pdf("docs/TextConferencingTut.pdf", "docs/TextConferencingTut.txt")
