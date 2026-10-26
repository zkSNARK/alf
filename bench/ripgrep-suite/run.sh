#!/bin/sh
# ripgrep's own benchmark suite (benchsuite/benchsuite, pinned below), with alf added to the
# single-file benchmarks it can answer.  alf.patch only adds alf commands; nothing else changes.
#
#   bench/ripgrep-suite/run.sh DATA_DIR OUT_PREFIX
#
# DATA_DIR holds the corpora (about 3.4 GB downloaded, 12 GB unpacked; reused if present).
# Only DATA_DIR/subtitles/en.sample.txt and ru.txt are searched.
# Writes OUT_PREFIX.csv (every sample), OUT_PREFIX.summary and OUT_PREFIX.versions.
# Needs alf, rg, GNU grep (as `grep`), ag and ugrep on PATH, plus python3, curl and patch.
set -e
REV=1be86392e0772fd0dbefcfbe57fa3ff15fc75da1  # the suite as used for ripgrep's 2022-12-16 published run
here=$(cd "$(dirname "$0")" && pwd)
data=$1; out=$2
[ -n "$data" ] && [ -n "$out" ] || { echo "usage: $0 DATA_DIR OUT_PREFIX" >&2; exit 2; }
grep -V 2>/dev/null | head -1 | grep -q GNU || { echo "grep on PATH must be GNU grep (macOS: PATH=\$(brew --prefix grep)/libexec/gnubin:\$PATH)" >&2; exit 2; }

work=$(mktemp -d)
curl -sSfL "https://raw.githubusercontent.com/BurntSushi/ripgrep/$REV/benchsuite/benchsuite" -o "$work/benchsuite"
patch -s -d "$work" -p2 < "$here/alf.patch"

# the searches use en.sample.txt (the first 55M lines of en.txt) and ru.txt: with those in place
# (copied from another machine, say), skip the download, which needs the whole of en.txt
if [ ! -f "$data/subtitles/en.sample.txt" ] || [ ! -f "$data/subtitles/ru.txt" ]; then
  python3 "$work/benchsuite" --dir "$data" --download subtitles-en --download subtitles-ru
fi
python3 "$here/check.py" "$data"  # alf prints exactly the lines rg and grep print

{ uname -srm; alf --simd-info | grep "in use"; rg --version | head -1; grep -V | head -1; ag --version | head -1; ugrep --version | head -1; } > "$out.versions"
python3 "$work/benchsuite" --dir "$data" --warmup-iter 3 --bench-iter 10 --force --raw "$out.csv" \
  '^subtitles_(en|ru)_(literal|alternate)$' | tee "$out.summary"
rm -rf "$work"
