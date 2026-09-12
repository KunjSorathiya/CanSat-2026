#!/usr/bin/env node
//
// Render one of this repository's Markdown documents to a .docx.
//
//     node tools/md_to_docx.js documentation/project/final-report.md out.docx
//
// It exists because the final report has to be submittable as a Word document and as a PDF,
// and the alternative -- maintaining the report twice -- guarantees the two disagree within
// a day. The Markdown in documentation/ is the single source; this is a renderer, not a
// second copy.
//
// It handles the subset this repository's Markdown actually uses: ATX headings, paragraphs,
// pipe tables, fenced code blocks, blockquotes (including GitHub callouts), ordered and
// unordered lists, horizontal rules, images, and inline bold / italic / code / links.
// Anything outside that subset is emitted as plain text rather than silently dropped.

const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel, Table, TableRow, TableCell,
  WidthType, ShadingType, BorderStyle, AlignmentType, ImageRun, PageBreak,
  Header, Footer, PageNumber, TableOfContents, LevelFormat, ExternalHyperlink,
} = require("docx");

// ---- page geometry -------------------------------------------------------------------
// A4 in DXA (1440 = 1 inch). The report is read on screen and printed in India, so A4
// rather than Letter.
const PAGE_W = 11906;
const PAGE_H = 16838;
const MARGIN = 1080;                       // 0.75"
const CONTENT_W = PAGE_W - 2 * MARGIN;     // usable width for tables and images

const INK = "212121";
const MUTED = "546E7A";
const VEHICLE = "0D47A1";
const RULE = "CFD8DC";
const CODE_BG = "F4F6F8";
const HEAD_BG = "ECEFF1";

// ---- inline markup -------------------------------------------------------------------

// Split inline Markdown into runs. Order matters: code spans are taken first so that a
// backtick span containing asterisks is not also italicised.
function inlineRuns(text, base = {}) {
  const runs = [];
  const re = /(`[^`]+`)|(\*\*\*[^*]+\*\*\*)|(\*\*[^*]+\*\*)|(\*[^*\n]+\*)|(\[[^\]]+\]\([^)]+\))/g;
  let last = 0;
  let m;
  while ((m = re.exec(text)) !== null) {
    if (m.index > last) runs.push(new TextRun({ text: unescapeMd(text.slice(last, m.index)), ...base }));
    const tok = m[0];
    if (tok.startsWith("`")) {
      runs.push(new TextRun({
        text: tok.slice(1, -1), font: "Consolas", size: 18,
        shading: { type: ShadingType.CLEAR, fill: CODE_BG }, ...base,
      }));
    } else if (tok.startsWith("***")) {
      // Recurse rather than emitting plain text: this repository routinely puts a code span
      // inside a bold span, and flattening it prints the backticks literally.
      runs.push(...inlineRuns(tok.slice(3, -3), { ...base, bold: true, italics: true }));
    } else if (tok.startsWith("**")) {
      runs.push(...inlineRuns(tok.slice(2, -2), { ...base, bold: true }));
    } else if (tok.startsWith("*")) {
      runs.push(...inlineRuns(tok.slice(1, -1), { ...base, italics: true }));
    } else {
      // A link: keep the label, and keep the target only when it points off the repository.
      const mm = /^\[([^\]]+)\]\(([^)]+)\)$/.exec(tok);
      const label = unescapeMd(mm[1]);
      const href = mm[2];
      if (/^https?:/.test(href)) {
        runs.push(new ExternalHyperlink({
          link: href,
          children: [new TextRun({ text: label, color: VEHICLE, underline: {}, ...base })],
        }));
      } else {
        runs.push(new TextRun({ text: label, ...base }));
      }
    }
    last = re.lastIndex;
  }
  if (last < text.length) runs.push(new TextRun({ text: unescapeMd(text.slice(last)), ...base }));
  return runs.length ? runs : [new TextRun({ text: "", ...base })];
}

function unescapeMd(s) {
  return s.replace(/\\([\\`*_{}[\]()#+\-.!])/g, "$1");
}

function plain(text) {
  return unescapeMd(text
    .replace(/`([^`]+)`/g, "$1")
    .replace(/\*\*\*([^*]+)\*\*\*/g, "$1")
    .replace(/\*\*([^*]+)\*\*/g, "$1")
    .replace(/\*([^*\n]+)\*/g, "$1")
    .replace(/\[([^\]]+)\]\([^)]+\)/g, "$1"));
}

// ---- block parsing -------------------------------------------------------------------

function splitRow(line) {
  return line.replace(/^\s*\|/, "").replace(/\|\s*$/, "").split("|").map((c) => c.trim());
}

function isDivider(line) {
  return /^\s*\|?[\s:|-]+\|[\s:|-]*$/.test(line) && line.includes("-");
}

// Column widths: proportional to the longest cell in each column, clamped so that a column
// of one-word headers cannot collapse and a prose column cannot eat the table.
function columnWidths(rows, total) {
  const n = Math.max(...rows.map((r) => r.length));
  const score = new Array(n).fill(1);
  for (const r of rows) {
    for (let i = 0; i < n; i += 1) {
      const len = plain(r[i] || "").length;
      score[i] = Math.max(score[i], Math.min(len, 90));
    }
  }
  const sum = score.reduce((a, b) => a + b, 0);
  const min = Math.floor(total * 0.07);
  let w = score.map((s) => Math.max(min, Math.floor((total * s) / sum)));
  // Renormalise after clamping, then push the rounding remainder into the widest column.
  const over = w.reduce((a, b) => a + b, 0) - total;
  if (over !== 0) {
    const widest = w.indexOf(Math.max(...w));
    w[widest] -= over;
  }
  return w;
}

function buildTable(rows, aligns) {
  // This repository writes several two-column "label / value" tables with an empty header
  // row, which Markdown renders as nothing and a shaded header row renders as a grey band
  // above the data. Drop it rather than shading emptiness.
  const headerless = rows.length > 1 && rows[0].every((c) => !c.trim());
  if (headerless) rows = rows.slice(1);

  const widths = columnWidths(rows, CONTENT_W);
  const border = { style: BorderStyle.SINGLE, size: 2, color: RULE };
  const trs = rows.map((cells, ri) => new TableRow({
    tableHeader: !headerless && ri === 0,
    children: widths.map((w, ci) => {
      const raw = cells[ci] === undefined ? "" : cells[ci];
      const align = aligns[ci] === "right" ? AlignmentType.RIGHT
        : aligns[ci] === "center" ? AlignmentType.CENTER : AlignmentType.LEFT;
      return new TableCell({
        width: { size: w, type: WidthType.DXA },
        shading: !headerless && ri === 0
          ? { type: ShadingType.CLEAR, fill: HEAD_BG } : undefined,
        margins: { top: 60, bottom: 60, left: 90, right: 90 },
        children: [new Paragraph({
          alignment: align,
          spacing: { before: 0, after: 0 },
          children: inlineRuns(raw,
            !headerless && ri === 0 ? { bold: true, size: 17 } : { size: 17 }),
        })],
      });
    }),
  }));
  return new Table({
    columnWidths: widths,
    width: { size: CONTENT_W, type: WidthType.DXA },
    borders: {
      top: border, bottom: border, left: border, right: border,
      insideHorizontal: border, insideVertical: border,
    },
    rows: trs,
  });
}

function codeParagraphs(lines) {
  // One paragraph per line, shaded, so a long listing still breaks across pages.
  return lines.map((l, i) => new Paragraph({
    shading: { type: ShadingType.CLEAR, fill: CODE_BG },
    spacing: { before: i === 0 ? 120 : 0, after: i === lines.length - 1 ? 160 : 0, line: 240 },
    indent: { left: 180, right: 180 },
    children: [new TextRun({ text: l || " ", font: "Consolas", size: 16, color: INK })],
  }));
}

function imageParagraphs(src, alt, baseDir) {
  const file = path.resolve(baseDir, src);
  if (!fs.existsSync(file)) {
    return [new Paragraph({
      spacing: { before: 120, after: 120 },
      children: [new TextRun({ text: `[missing image: ${src}]`, italics: true, color: MUTED })],
    })];
  }
  // Fit the content width, preserving aspect from the PNG's IHDR.
  const buf = fs.readFileSync(file);
  const pxW = buf.readUInt32BE(16);
  const pxH = buf.readUInt32BE(20);
  const maxPt = 468;                       // content width in points at these margins
  const w = Math.min(maxPt, pxW / 2.6);
  const h = (pxH / pxW) * w;
  return [new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 180, after: 60 },
    children: [new ImageRun({ type: "png", data: buf, transformation: { width: w, height: h } })],
  })];
}

// ---- the main pass -------------------------------------------------------------------

function convert(md, baseDir) {
  const lines = md.split(/\r?\n/);
  const out = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];

    // fenced code
    if (/^\s*```/.test(line)) {
      const body = [];
      i += 1;
      while (i < lines.length && !/^\s*```/.test(lines[i])) { body.push(lines[i]); i += 1; }
      i += 1;
      out.push(...codeParagraphs(body));
      continue;
    }

    // raw HTML wrappers used for centring in the Markdown: skip the tags, keep the content
    if (/^\s*<\/?div/.test(line) || /^\s*<!--/.test(line)) { i += 1; continue; }

    // horizontal rule
    if (/^\s*---+\s*$/.test(line)) {
      out.push(new Paragraph({
        spacing: { before: 120, after: 120 },
        border: { bottom: { style: BorderStyle.SINGLE, size: 6, color: RULE } },
        children: [new TextRun({ text: "" })],
      }));
      i += 1;
      continue;
    }

    // heading
    const h = /^(#{1,6})\s+(.*)$/.exec(line);
    if (h) {
      const level = h[1].length;
      const levels = [HeadingLevel.HEADING_1, HeadingLevel.HEADING_2, HeadingLevel.HEADING_3,
        HeadingLevel.HEADING_4, HeadingLevel.HEADING_5, HeadingLevel.HEADING_6];
      out.push(new Paragraph({
        heading: levels[level - 1],
        spacing: { before: level <= 2 ? 320 : 240, after: 120 },
        pageBreakBefore: false,
        children: inlineRuns(h[2]),
      }));
      i += 1;
      continue;
    }

    // image on its own line
    const img = /^\s*!\[([^\]]*)\]\(([^)]+)\)\s*$/.exec(line);
    if (img) { out.push(...imageParagraphs(img[2], img[1], baseDir)); i += 1; continue; }

    // table
    if (/^\s*\|/.test(line) && i + 1 < lines.length && isDivider(lines[i + 1])) {
      const header = splitRow(line);
      const aligns = splitRow(lines[i + 1]).map((c) => (
        c.endsWith(":") && c.startsWith(":") ? "center" : c.endsWith(":") ? "right" : "left"));
      const rows = [header];
      i += 2;
      while (i < lines.length && /^\s*\|/.test(lines[i])) { rows.push(splitRow(lines[i])); i += 1; }
      out.push(buildTable(rows, aligns));
      out.push(new Paragraph({ spacing: { after: 160 }, children: [new TextRun({ text: "" })] }));
      continue;
    }

    // blockquote, including GitHub callouts
    if (/^\s*>/.test(line)) {
      const body = [];
      while (i < lines.length && /^\s*>/.test(lines[i])) {
        body.push(lines[i].replace(/^\s*>\s?/, ""));
        i += 1;
      }
      let label = null;
      if (body.length && /^\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]/i.test(body[0])) {
        label = /^\[!(\w+)\]/.exec(body[0])[1].toUpperCase();
        body.shift();
      }
      const chunks = body.join("\n").split(/\n\s*\n/).filter((c) => c.trim());
      if (label) {
        out.push(new Paragraph({
          spacing: { before: 160, after: 40 },
          indent: { left: 260 },
          border: { left: { style: BorderStyle.SINGLE, size: 18, color: VEHICLE, space: 12 } },
          children: [new TextRun({ text: label, bold: true, size: 17, color: VEHICLE })],
        }));
      }
      chunks.forEach((c, ci) => out.push(new Paragraph({
        spacing: { before: label || ci ? 40 : 160, after: ci === chunks.length - 1 ? 160 : 40 },
        indent: { left: 260 },
        border: { left: { style: BorderStyle.SINGLE, size: 18, color: VEHICLE, space: 12 } },
        children: inlineRuns(c.replace(/\n/g, " "), { italics: !label }),
      })));
      continue;
    }

    // lists
    const ITEM = /^(\s*)([-*+]|\d+[.)])\s+(.*)$/;
    if (ITEM.test(line)) {
      while (i < lines.length) {
        const m2 = ITEM.exec(lines[i]);
        if (!m2) break;
        const depth = Math.floor(m2[1].length / 2);
        const ordered = /\d/.test(m2[2]);
        // Soft-wrapped continuation lines belong to THIS item, not to a paragraph of their
        // own. Emitting them separately indents each wrapped line as if it were nested,
        // which is how a three-line bullet ends up looking like four list levels.
        const parts = [m2[3]];
        i += 1;
        while (i < lines.length && lines[i].trim()
               && !ITEM.test(lines[i])
               && /^\s{2,}\S/.test(lines[i])
               && !/^\s*(#{1,6}\s|\||>|```)/.test(lines[i])) {
          parts.push(lines[i].trim());
          i += 1;
        }
        out.push(new Paragraph({
          numbering: { reference: ordered ? "md-ordered" : "md-bullet", level: Math.min(depth, 2) },
          spacing: { before: 40, after: 40 },
          children: inlineRuns(parts.join(" ").replace(/\s+/g, " ").trim()),
        }));
      }
      continue;
    }

    // blank
    if (!line.trim()) { i += 1; continue; }

    // paragraph: join soft-wrapped lines
    const para = [line];
    i += 1;
    while (i < lines.length && lines[i].trim()
           && !/^\s*(#{1,6}\s|\||>|```|---+\s*$|!\[)/.test(lines[i])
           && !/^(\s*)([-*+]|\d+[.)])\s+/.test(lines[i])
           && !/^\s*<\/?div/.test(lines[i])) {
      para.push(lines[i]);
      i += 1;
    }
    out.push(new Paragraph({
      spacing: { before: 60, after: 140, line: 276 },
      children: inlineRuns(para.join(" ").replace(/\s+/g, " ").trim()),
    }));
  }
  return out;
}

// ---- document ------------------------------------------------------------------------

function main() {
  const [, , src, dst] = process.argv;
  if (!src || !dst) {
    console.error("usage: node tools/md_to_docx.js <input.md> <output.docx>");
    return 1;
  }
  const md = fs.readFileSync(src, "utf8");
  const baseDir = path.dirname(path.resolve(src));
  const children = convert(md, baseDir);

  const doc = new Document({
    creator: "CanSat 2026 — Team CAN-Team-25",
    title: "CanSat 2026 — Final Project Report",
    description: "Final project report: mission, design, simulations, firmware, testing and results.",
    styles: {
      default: {
        document: { run: { font: "Calibri", size: 20, color: INK }, paragraph: { spacing: { line: 276 } } },
        heading1: { run: { font: "Calibri", size: 40, bold: true, color: INK },
          paragraph: { spacing: { before: 360, after: 160 } } },
        heading2: { run: { font: "Calibri", size: 30, bold: true, color: VEHICLE },
          paragraph: { spacing: { before: 340, after: 140 } } },
        heading3: { run: { font: "Calibri", size: 24, bold: true, color: INK },
          paragraph: { spacing: { before: 260, after: 100 } } },
        heading4: { run: { font: "Calibri", size: 21, bold: true, color: MUTED },
          paragraph: { spacing: { before: 200, after: 80 } } },
      },
    },
    numbering: {
      config: [
        { reference: "md-bullet",
          levels: [0, 1, 2].map((l) => ({
            level: l, format: LevelFormat.BULLET, text: ["•", "◦", "▪"][l],
            alignment: AlignmentType.LEFT,
            style: { paragraph: { indent: { left: 400 + 360 * l, hanging: 260 } } },
          })) },
        { reference: "md-ordered",
          levels: [0, 1, 2].map((l) => ({
            level: l, format: LevelFormat.DECIMAL, text: `%${l + 1}.`,
            alignment: AlignmentType.LEFT,
            style: { paragraph: { indent: { left: 400 + 360 * l, hanging: 260 } } },
          })) },
      ],
    },
    sections: [{
      properties: {
        page: { size: { width: PAGE_W, height: PAGE_H },
          margin: { top: MARGIN, bottom: MARGIN, left: MARGIN, right: MARGIN } },
      },
      headers: {
        default: new Header({ children: [new Paragraph({
          alignment: AlignmentType.RIGHT,
          border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: RULE, space: 6 } },
          children: [new TextRun({
            text: "CanSat 2026 — Final Project Report · Team CAN-Team-25",
            size: 15, color: MUTED,
          })],
        })] }),
      },
      footers: {
        default: new Footer({ children: [new Paragraph({
          alignment: AlignmentType.CENTER,
          children: [new TextRun({ children: ["Page ", PageNumber.CURRENT, " of ", PageNumber.TOTAL_PAGES],
            size: 15, color: MUTED })],
        })] }),
      },
      children,
    }],
  });

  return Packer.toBuffer(doc).then((buf) => {
    fs.writeFileSync(dst, buf);
    console.log(`wrote ${dst} (${(buf.length / 1024).toFixed(0)} KB, ${children.length} blocks)`);
    return 0;
  });
}

Promise.resolve(main()).then((c) => process.exit(c || 0)).catch((e) => {
  console.error(e);
  process.exit(1);
});
