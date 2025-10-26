#!/bin/sh
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR"

MDVIC=${MDVIC:-../mdvic}

WIDTH=${WIDTH:-40}
export MDVIC_NO_COLOR=1
export MDVIC_NO_OSC8=1

pass=0
fail=0

run_case() {
  base=$1
  mdfile="cases/$base.md"
  out_golden="golden/$base.out"
  err_golden="golden/$base.err"
  envfile="cases/$base.env"

  printf "[TEST] %s... " "$base"

  out_actual="/tmp/mdvic_${base}_out.$$"
  err_actual="/tmp/mdvic_${base}_err.$$"

  if [ -f "$envfile" ]; then
    EXTRA_ENV=$(sed -e 's/#.*$//' -e '/^$/d' "$envfile" | tr '\n' ' ')
  else
    EXTRA_ENV=""
  fi

  if ! eval $EXTRA_ENV "$MDVIC" --no-color --wrap --width "$WIDTH" "$mdfile" >"$out_actual" 2>"$err_actual"; then
    echo "(mdvic exited non-zero)"
  fi

  ok=1
  if [ -f "$out_golden" ]; then
    if ! diff -u "$out_golden" "$out_actual" >/dev/null 2>&1; then
      echo "FAIL (stdout)"; ok=0
      echo "--- diff (stdout) ---"
      diff -u "$out_golden" "$out_actual" || true
    fi
  fi
  if [ -f "$err_golden" ]; then
    if ! diff -u "$err_golden" "$err_actual" >/dev/null 2>&1; then
      [ "$ok" -eq 1 ] && echo "FAIL (stderr)" || true
      ok=0
      echo "--- diff (stderr) ---"
      diff -u "$err_golden" "$err_actual" || true
    fi
  fi

  rm -f "$out_actual" "$err_actual"

  if [ "$ok" -eq 1 ]; then
    echo "OK"; pass=$((pass+1))
  else
    fail=$((fail+1))
  fi
}

# List of base case names (without extension)
CASES="\
01_paragraph \
02_heading \
03_emph_code_link \
04_blockquote \
05_list_wrap \
06_table_basic \
07_table_align \
08_table_mismatch \
09_fence_unclosed \
10_inline_backtick \
11_unicode_width \
12_nested_lists \
13_nested_quotes \
14_code_fence \
15_no_ansi_sanity \
16_math_unicode \
17_math_ascii"

for b in $CASES; do
  run_case "$b"
done

echo "\nSummary: $pass passed, $fail failed"
test "$fail" -eq 0
