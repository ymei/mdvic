#!/usr/bin/awk -f
# Merge sorted numeric ranges. Input: "START END" decimal, sorted by START asc.
# Output: merged ranges as "START END".

NF >= 2 {
  s = $1 + 0; e = $2 + 0
  if (!initialized) {
    cs = s; ce = e; initialized = 1; next
  }
  if (s <= ce + 1) {
    if (e > ce) ce = e
  } else {
    printf "%d %d\n", cs, ce
    cs = s; ce = e
  }
}

END {
  if (initialized) printf "%d %d\n", cs, ce
}

