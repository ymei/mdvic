#!/usr/bin/awk -f
# Usage:
#  awk -v MODE=unicode -f tools/gen_wcwidth.awk UnicodeData.txt
#  awk -v MODE=eaw     -f tools/gen_wcwidth.awk EastAsianWidth.txt
# Outputs lines: "START END" as decimal codepoints

function hex2dec(h,   x) {
  return strtonum("0x" h)
}

BEGIN {
  mode = (MODE == "eaw") ? "eaw" : "unicode"
  FS = (mode == "unicode") ? ";" : "[ ;#]"
  pending_first = -1
  pending_gc = ""
}

# UnicodeData.txt parser for combining marks
mode == "unicode" {
  if ($0 ~ /^#/ || $0 ~ /^\s*$/) next
  code = $1
  name = $2
  gc = $3
  cp = hex2dec(code)

  if (name ~ /, First>$/) {
    pending_first = cp
    pending_gc = gc
    next
  }
  if (name ~ /, Last>$/) {
    if (pending_first >= 0 && (pending_gc == "Mn" || pending_gc == "Me")) {
      printf "%d %d\n", pending_first, cp
    }
    pending_first = -1
    pending_gc = ""
    next
  }
  if (gc == "Mn" || gc == "Me") {
    printf "%d %d\n", cp, cp
  }
  next
}

# EastAsianWidth.txt parser for W/F (wide/fullwidth)
mode == "eaw" {
  if ($0 ~ /^#/ || $0 ~ /^\s*$/) next
  # token 1 is range or single, token 2 is class
  token = $1
  cls = $2
  if (cls != "W" && cls != "F") next
  split(token, a, /\.\./)
  if (length(a) == 2) {
    s = hex2dec(a[1]); e = hex2dec(a[2])
  } else {
    s = hex2dec(token); e = s
  }
  printf "%d %d\n", s, e
  next
}

