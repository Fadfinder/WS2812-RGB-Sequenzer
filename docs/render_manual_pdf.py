from pathlib import Path

from docx import Document
from docx.table import Table as DocxTable
from docx.text.paragraph import Paragraph
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_RIGHT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    Image,
    PageBreak,
    PageTemplate,
    Paragraph as PdfParagraph,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "WS2812_RGB_Sequenzer_Benutzerhandbuch.docx"
OUTPUT = ROOT / "docs" / "WS2812_RGB_Sequenzer_Benutzerhandbuch.pdf"
WIRING = ROOT / "docs" / "wemos_d1_mini_ws2812b_wiring.png"


def esc(text):
    return (text or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def iter_blocks(document):
    for child in document.element.body.iterchildren():
        if child.tag.endswith("}p"):
            yield Paragraph(child, document)
        elif child.tag.endswith("}tbl"):
            yield DocxTable(child, document)


def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 8.5)
    canvas.setFillColor(colors.HexColor("#5A6370"))
    canvas.drawRightString(letter[0] - inch, letter[1] - 0.47 * inch, "WS2812 RGB Sequenzer | Benutzerhandbuch")
    canvas.drawRightString(letter[0] - inch, 0.42 * inch, f"Seite {doc.page}")
    canvas.restoreState()


def build():
    src = Document(SOURCE)
    styles = getSampleStyleSheet()
    body = ParagraphStyle("Body", parent=styles["BodyText"], fontName="Helvetica", fontSize=9.6, leading=12, spaceAfter=6, textColor=colors.HexColor("#172033"))
    h1 = ParagraphStyle("H1", parent=body, fontName="Helvetica-Bold", fontSize=15, leading=18, spaceBefore=12, spaceAfter=8, textColor=colors.HexColor("#2E74B5"), keepWithNext=True)
    h2 = ParagraphStyle("H2", parent=body, fontName="Helvetica-Bold", fontSize=12.5, leading=15, spaceBefore=10, spaceAfter=6, textColor=colors.HexColor("#2E74B5"), keepWithNext=True)
    h3 = ParagraphStyle("H3", parent=body, fontName="Helvetica-Bold", fontSize=11.5, leading=14, spaceBefore=8, spaceAfter=5, textColor=colors.HexColor("#1F4D78"), keepWithNext=True)
    bullet = ParagraphStyle("Bullet", parent=body, leftIndent=16, firstLineIndent=-9, bulletIndent=5, spaceAfter=4)
    number = ParagraphStyle("Number", parent=body, leftIndent=18, firstLineIndent=-12, bulletIndent=2, spaceAfter=4)
    cover_kicker = ParagraphStyle("CoverKicker", parent=body, fontName="Helvetica-Bold", fontSize=10, textColor=colors.HexColor("#2E74B5"), alignment=TA_CENTER, spaceAfter=12)
    cover_title = ParagraphStyle("CoverTitle", parent=body, fontName="Helvetica-Bold", fontSize=27, leading=32, textColor=colors.HexColor("#1F4D78"), alignment=TA_CENTER, spaceAfter=9)
    cover_sub = ParagraphStyle("CoverSub", parent=body, fontSize=14, leading=18, textColor=colors.HexColor("#2E74B5"), alignment=TA_CENTER, spaceAfter=38)
    cover_note = ParagraphStyle("CoverNote", parent=body, fontName="Helvetica-Oblique", fontSize=10.5, textColor=colors.HexColor("#5A6370"), alignment=TA_CENTER)
    cover_date = ParagraphStyle("CoverDate", parent=body, fontSize=9.5, textColor=colors.HexColor("#5A6370"), alignment=TA_CENTER)
    cell = ParagraphStyle("Cell", parent=body, fontSize=8.7, leading=10.8, spaceAfter=0)
    cell_head = ParagraphStyle("CellHead", parent=cell, fontName="Helvetica-Bold", textColor=colors.HexColor("#1F4D78"))

    pdf = BaseDocTemplate(str(OUTPUT), pagesize=letter, leftMargin=inch, rightMargin=inch, topMargin=0.75 * inch, bottomMargin=0.67 * inch, title="WS2812 RGB Sequenzer - Benutzerhandbuch", author="WS2812 RGB Sequenzer Projekt")
    frame = Frame(pdf.leftMargin, pdf.bottomMargin, pdf.width, pdf.height, id="normal")
    pdf.addPageTemplates(PageTemplate(id="manual", frames=frame, onPage=header_footer))
    story = []
    list_counter = 0
    cover_index = 0
    wiring_added = False

    for block in iter_blocks(src):
        if isinstance(block, DocxTable):
            data = []
            for row_index, row in enumerate(block.rows):
                data.append([PdfParagraph(esc(c.text), cell_head if row_index == 0 else cell) for c in row.cells])
            if data:
                col_widths = [6.5 * inch] if len(data[0]) == 1 else [1.8 * inch, 4.7 * inch]
                table = Table(data, colWidths=col_widths, repeatRows=1 if len(data[0]) > 1 else 0, hAlign="LEFT")
                table.setStyle(TableStyle([
                    ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#E8EEF5")),
                    ("GRID", (0, 0), (-1, -1), 0.45, colors.HexColor("#B8C3D1")),
                    ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                    ("LEFTPADDING", (0, 0), (-1, -1), 6),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                    ("TOPPADDING", (0, 0), (-1, -1), 5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
                ]))
                story.extend([table, Spacer(1, 7)])
            continue

        text = block.text.strip()
        style_name = block.style.name if block.style else "Normal"
        if block._p.xpath('.//w:br[@w:type="page"]'):
            story.append(PageBreak())
            list_counter = 0
            continue
        if not text:
            if block._p.xpath(".//w:drawing") and not wiring_added and WIRING.exists():
                story.append(Image(str(WIRING), width=6.4 * inch, height=3.92 * inch))
                story.append(Spacer(1, 8))
                wiring_added = True
            continue

        if cover_index < 5:
            if cover_index == 0:
                story.extend([Spacer(1, 78), PdfParagraph(esc(text), cover_kicker)])
            elif cover_index == 1:
                story.append(PdfParagraph(esc(text), cover_title))
            elif cover_index == 2:
                story.append(PdfParagraph(esc(text), cover_sub))
            elif cover_index == 3:
                story.extend([Spacer(1, 40), PdfParagraph(esc(text), cover_note)])
            else:
                story.extend([Spacer(1, 112), PdfParagraph(esc(text), cover_date)])
            cover_index += 1
            continue

        if style_name == "Heading 1":
            story.append(PdfParagraph(esc(text), h1))
            if text == "3. Verdrahtung" and WIRING.exists() and not wiring_added:
                story.append(Image(str(WIRING), width=6.4 * inch, height=3.92 * inch))
                story.append(Spacer(1, 8))
                wiring_added = True
            list_counter = 0
        elif style_name == "Heading 2":
            story.append(PdfParagraph(esc(text), h2))
        elif style_name == "Heading 3":
            story.append(PdfParagraph(esc(text), h3))
        elif style_name == "List Bullet":
            story.append(PdfParagraph(esc(text), bullet, bulletText="•"))
            list_counter = 0
        elif style_name == "List Number":
            list_counter += 1
            story.append(PdfParagraph(esc(text), number, bulletText=f"{list_counter}."))
        else:
            list_counter = 0
            story.append(PdfParagraph(esc(text), body))

    pdf.build(story)
    print(OUTPUT)


if __name__ == "__main__":
    build()
