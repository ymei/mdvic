# Development Plan

This document tracks milestones and roadmap items for mdvic.

## M0 — Parser and core renderer

- Integrate `cmark` and implement block/inline rendering.
- Add ANSI style theme and no-color mode.
- Ship initial `wcwidth()` and wrapping.

## M1 — Tables and hyperlinks

- Two-pass GFM table layout and alignment.
- OSC-8 hyperlinks with detection and fallback.

## M2 — Math subset v1

- `$…$` and `$$…$$` scanning with nesting guards.
- Unicode path for Greek, super/subscripts, `\frac`, `\sqrt`.
- ASCII fallback path and configuration flag.

## M3 — Matrices and stacked fractions

- `pmatrix` rendering with width-aware cell padding.
- Multi-line fraction layout when needed.

## M4 — Robustness and test coverage

- Adopt CommonMark tests into CI.
- Add golden tests for math and tables across width modes.
- Unicode table update tool and documented process.

## M5 — Packaging

- Release tarballs.
- Man page `mdvic(1)` and shell completions.
- Homebrew, AUR, and Debian packaging recipes.

