# mdvic

A minimal terminal renderer for Markdown with inline and display math.  Implemented in C.  Uses `cmark` for parsing and a compact Unicode math layer for `$…$` and `$$…$$`.  Outputs ANSI suitable for any TTY or `less -R`.  No browser, no TeX engine, no temporary files.

## Purpose

Provide a single static binary that:

* Parses CommonMark via `cmark`.
* Renders Markdown to ANSI with correct width accounting and soft wrapping.
* Converts a strict TeX-style math subset to readable Unicode or ASCII for terminals.

Target users work primarily in terminals or within Emacs shells and need fast, dependency-light previews.  Exact LaTeX page layout is out of scope.  Prose and documentation fidelity is the priority.

## Non-Goals

* Full LaTeX emulation, macro expansion, or page layout.
* HTML rendering inside Markdown.
* Inline images or graphics.
* Heavy runtime dependencies, daemons, or network access.

## Features

* CommonMark parsing using `cmark` (vendored or linked).
* ANSI renderer with an internal `wcwidth()` table for Unicode correctness.
* OSC-8 terminal hyperlinks with safe fallbacks.
* GFM-style tables with two-pass width measurement and padding.
* Math subset: Greek and common symbols, superscripts/subscripts, `\frac`, `\sqrt`, small matrices.
* Configurable color, output width, and math rendering mode (Unicode or ASCII).

## Quick Start

```sh
# clone and build (example; exact Makefile targets may differ)
git clone https://example.com/mdvic.git
cd mdvic
make

# render to TTY (pager recommended)
./mdvic README.md | less -RS

# direct view without pager
./mdvic notes.md
```

Environment variables:

```sh
MDVIC_NO_COLOR=1        # disable ANSI colors
MDVIC_WIDTH=100         # override detected terminal width
MDVIC_MATH=ascii        # ASCII fallbacks instead of Unicode math
MDVIC_WRAP=1            # enable pre-wrapping (default: no-wrap)
MDVIC_NO_WRAP=1         # force-disable pre-wrapping
MDVIC_NO_OSC8=1         # disable OSC-8 hyperlinks
```

## CLI

```
mdvic [--no-color] [--no-lint] [--wrap|--no-wrap] [--width N] [--math {unicode|ascii}] [--no-osc8] [FILE...]
```

* No file means read stdin.
* Multiple files render sequentially with a separator line.
* No-wrap is the default. Use `--wrap --width N` to pre-wrap; otherwise let your terminal/pager handle wrapping. With `less`, `-S` prevents hard wrapping.

## Linting

mdvic performs a lightweight lint pass before rendering and reports issues to stderr.
This helps catch common formatting mistakes early without blocking output.

- Unclosed fenced code blocks (``` or ~~~)
- Unmatched inline code backticks on a line
- GFM table inconsistencies (column count/alignment)

Lint messages use the format `FILE:LINE: message`. Rendering continues even when
issues are found.

Disable linting with `--no-lint` or `MDVIC_NO_LINT=1`.

## Math behavior

Inline `$…$` and display `$$…$$` are detected in the text stream.  mdvic parses a strict subset and emits Unicode glyphs and combining marks or ASCII fallbacks.  When a terminal font lacks a glyph, mdvic substitutes a legible ASCII construction.

Rendered example: ( ∇^{2}\phi = −ρ/ε_{0} ).
LaTeX source:

```tex
\nabla^{2}\phi = -\frac{\rho}{\varepsilon_{0}}
```

Rendered example: ( \displaystyle \frac{d}{dx}\sqrt{x} = \frac{1}{2\sqrt{x}} ).
LaTeX source:

```tex
\frac{d}{dx}\sqrt{x} = \frac{1}{2\sqrt{x}}
```

## Architecture

* **Parser (`cmark`)**.  Produces an AST for CommonMark.  mdvic vendors `cmark` by default to avoid external installation.
* **Renderer**.  Maps blocks and spans to ANSI with a small formatting stack.  Handles headings, paragraphs, lists, block quotes, and code fences.
* **Wrapper**.  ANSI-aware word wrapping that ignores escape sequences and uses a bundled Unicode width table.
* **Unicode width tables**.  Internal `wcwidth()` with East Asian and combining-mark handling.  Tables are versioned to keep rendering stable across platforms.
* **Math layer**.  Inline detector for `$…$` and `$$…$$`.  Recursive-descent parser for the subset.  Emits Unicode or ASCII according to configuration.
* **Tables**.  Two-pass rendering for GFM tables to compute column widths, then pad cells consistently.
* **Term capabilities**.  Detects color depth and OSC-8 hyperlink support via environment heuristics with opt-out flags.

## Supported math subset (initial)

* Greek and symbols: `\alpha … \Omega`, `\pm`, `\times`, `\cdot`, `\partial`, `\nabla`, `\infty`, `\le`, `\ge`, `\neq`.
* Superscripts and subscripts: `x^{2}`, `x_i`, nested with `{}`.
* Fractions: `\frac{a}{b}` with compact `a⁄b` or two-line layout for long terms.
* Roots: `\sqrt{x}` and `\sqrt[n]{x}` rendered as `ⁿ√(x)`.
* Small matrices: `\begin{pmatrix} … \end{pmatrix}` with monospaced cells.

Out of scope for v1: `\overbrace`, large operators with limits (`\sum_{…}` on multiple lines), alignment environments, and user macros.

## Terminal behavior

* **Colors**.  Uses standard or 256-color ANSI.  Disable with `--no-color` or `MDVIC_NO_COLOR=1`.
* **Links**.  Emits OSC-8 hyperlinks when supported by the terminal, else prints `text (URL)`.
* **Paging**.  By default mdvic does not soft-wrap; use `less -RS` to preserve alignment and avoid hard wrapping. Enable pre-wrap with `--wrap --width N` if desired.

## Build

The default build vendors `cmark` into `third_party/cmark/` and compiles mdvic as a single binary.

```sh
# typical static build on Linux (example)
cc -O2 -static third_party/cmark/src/*.c src/*.c -o mdvic
```

Library and platform notes:

* No runtime dependencies beyond libc.
* `wcwidth()` tables are compiled in.
* On systems without static libc, the Makefile falls back to dynamic linking.

## Testing

* CommonMark spec tests: import the official suite and run all non-HTML cases.
* GFM tables: fixtures for width measurement, alignment, and wide glyphs.
* Unicode width: snapshot tests for combining marks, East Asian wide characters, and emoji ZWJ fallbacks.
* Math: golden files for each construct in both Unicode and ASCII modes.

## Performance targets

* Cold start under 5 ms on typical x86-64.
* Throughput ≥ 50 MB/s on plain text without math.
* O(1) additional memory per nesting level during rendering, aside from table and math buffers.

## Portability

* Linux, macOS, and BSDs are first-class.  Windows support targets MSYS2 and WSL.
* Terminals tested: xterm, tmux, iTerm2, kitty, Alacritty, Linux console.  Behavior degrades gracefully when OSC-8 or 256-color is unavailable.

## Development plan

**M0 — Parser and core renderer**

* Integrate `cmark` and implement block/inline rendering.
* Add ANSI style theme and no-color mode.
* Ship initial `wcwidth()` and wrapping.

**M1 — Tables and hyperlinks**

* Two-pass GFM table layout and alignment.
* OSC-8 hyperlinks with detection and fallback.

**M2 — Math subset v1**

* `$…$` and `$$…$$` scanning with nesting guards.
* Unicode path for Greek, super/subscripts, `\frac`, `\sqrt`.
* ASCII fallback path and configuration flag.

**M3 — Matrices and stacked fractions**

* `pmatrix` rendering with width-aware cell padding.
* Multi-line fraction layout when needed.

**M4 — Robustness and test coverage**

* Adopt CommonMark tests into CI.
* Add golden tests for math and tables across width modes.
* Unicode table update tool and documented process.

**M5 — Packaging**

* Release tarballs.
* Man page `mdvic(1)` and shell completions.
* Homebrew, AUR, and Debian packaging recipes.

## Contributing

Keep changes small and well-scoped.  Include tests for any new Markdown construct or math token you add.  For Unicode additions, include width snapshots for at least two terminals.  Discuss breaking changes in an issue before opening a PR.

## License

MIT is preferred.  BSD-2 is acceptable.  Choose one before the first tagged release.
