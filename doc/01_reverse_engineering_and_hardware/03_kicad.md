# KiCad

## Purpose

This section tracks the custom hardware files that belong to the adapted dryer controller setup.

## Available design assets

The repository already contains KiCad project files for the custom interface boards and variants.

Archived path:

- `doc/archive/pre_reorg_v0.7.1/legacy_docs/hardware/kicad/Filament-Silicagel-Dryer/`

Available project artifacts include:

- `.kicad_sch`
- `.kicad_pcb`
- `.kicad_pro`
- generated PDFs and gerbers
- alternative lochraster variants

## Recommended documentation baseline

The KiCad documentation should eventually describe:

1. which board is the currently valid build target
2. how the `HOST`, `CLIENT` and power board are wired together
3. which variants are legacy and which remain supported
4. where the series resistors, UART lines and sensor connections are placed

## Immediate interpretation of the current repository

Right now the KiCad data is valuable but not yet normalized. The archive keeps all variants, while the active documentation focuses on architecture first.
