#!/bin/sh
# Usage: prepare-exe.sh <Juiced.exe>: work/Juiced.exe, the lifter's input, from Juiced 1.00's EXE without copy
# protection (sha256 below). Needs Python 3 with capstone and unicorn (a venv in .venv is made for it).
set -eu
cd "$(dirname "$0")/.."
[ -x .venv/bin/python ] || { python3 -m venv .venv && .venv/bin/pip install -q capstone unicorn; }
mkdir -p work
.venv/bin/python tools/prepare_exe.py "$1" prep/instructions.txt.gz work/Juiced.exe work/prepare-report.tsv >/dev/null
echo "9fdb59eae13efd252b047d9e74fda8a93dd79f811a8f57eb84ae1e5ebbd27365  work/Juiced.exe" | shasum -a 256 -c -
