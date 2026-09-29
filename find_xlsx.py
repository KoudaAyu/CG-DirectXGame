import os
import sys
import openpyxl
from openpyxl.styles import Alignment, Font

downloads = r"C:\Users\3329a\Downloads"
for fname in os.listdir(downloads):
    if fname.endswith(".xlsx"):
        full = os.path.join(downloads, fname)
        sys.stdout.buffer.write(f"Found xlsx: {fname}\n".encode("utf-8"))
        try:
            wb = openpyxl.load_workbook(full)
            ws = wb.active
            sys.stdout.buffer.write(f"  Title: {ws.title}, G4: {ws['G4'].value}\n".encode("utf-8"))
        except Exception as e:
            sys.stdout.buffer.write(f"  Error: {e}\n".encode("utf-8"))
