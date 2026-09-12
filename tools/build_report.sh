#!/usr/bin/env bash
#
# Rebuild the final project report's figures and its .docx / .pdf from the Markdown source.
#
#     bash tools/build_report.sh
#
# The Markdown at documentation/project/final-report.md is the single source. The Word and
# PDF files are generated from it, and they are committed so that a submission does not
# depend on anybody having Node, matplotlib or Word installed.
#
# Requirements, and each step is skipped with a warning rather than failing the script if
# its tool is absent -- the point is that a partial rebuild is better than none:
#
#   figures  python + matplotlib
#   .docx    node + the `docx` package (npm install docx)
#   .pdf     Microsoft Word via PowerShell COM, or LibreOffice `soffice`
#
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SRC="documentation/project/final-report.md"
DOCX="documentation/project/CanSat-2026-Final-Report.docx"
PDF="documentation/project/CanSat-2026-Final-Report.pdf"

python_bin() {
  for c in python3 python py; do
    if command -v "$c" >/dev/null 2>&1; then echo "$c"; return 0; fi
  done
  return 1
}

# ---- 1. figures -----------------------------------------------------------------------
echo "== report figures =="
PY="$(python_bin)" || { echo "  SKIP: no python on PATH"; PY=""; }
if [ -n "$PY" ]; then
  if "$PY" -c "import matplotlib" >/dev/null 2>&1; then
    "$PY" tools/gen_report_figures.py || exit 1
  else
    echo "  SKIP: matplotlib not installed; keeping the committed figures"
  fi
fi

# ---- 2. the Word document ---------------------------------------------------------------
echo "== $DOCX =="
if command -v node >/dev/null 2>&1; then
  if ! node -e "require('docx')" >/dev/null 2>&1; then
    echo "  the 'docx' package is not resolvable; trying npm install"
    npm install --no-save docx >/dev/null 2>&1 || true
  fi
  if node -e "require('docx')" >/dev/null 2>&1; then
    node tools/md_to_docx.js "$SRC" "$DOCX" || exit 1
  else
    echo "  SKIP: 'docx' unavailable. Run: npm install docx"
  fi
else
  echo "  SKIP: node not on PATH"
fi

# ---- 3. the PDF -------------------------------------------------------------------------
echo "== $PDF =="
if [ ! -f "$DOCX" ]; then
  echo "  SKIP: no .docx to convert"
elif command -v soffice >/dev/null 2>&1; then
  soffice --headless --convert-to pdf --outdir "$(dirname "$PDF")" "$DOCX" >/dev/null \
    && echo "  wrote $PDF (LibreOffice)"
elif command -v powershell.exe >/dev/null 2>&1; then
  # Word is the better renderer on Windows and it evaluates the page-number field, which a
  # headless converter can leave showing as "Page 1 of 1".
  powershell.exe -NoProfile -NonInteractive -Command "
    \$w = New-Object -ComObject Word.Application
    \$w.Visible = \$false; \$w.DisplayAlerts = 0
    try {
      \$d = \$w.Documents.Open('$ROOT\\$DOCX'.Replace('/','\\'), \$false, \$true)
      \$d.Fields.Update() | Out-Null
      \$d.SaveAs([ref]('$ROOT\\$PDF'.Replace('/','\\')), [ref]17)
      Write-Output ('  wrote $PDF, ' + \$d.ComputeStatistics(2) + ' pages (Word)')
      \$d.Close(\$false)
    } finally { \$w.Quit() }" || echo "  SKIP: Word conversion failed"
else
  echo "  SKIP: neither soffice nor powershell available"
fi

echo "done"
