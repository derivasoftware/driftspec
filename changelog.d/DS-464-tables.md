A table is placed by one engine of the layout too (DS-464, step 4), as the Owner approved from the before/after
renders: `layout::GridEngine`, behind `ITableEngine` and named by `EngineFactory::table`, places a `TableGraph` (the
corner, the headings, the rows, each text as its words in characters) with the SVG's proportions, so a cell longer than
40 characters wraps between words in every sink, and a column takes its room each side. One `TableComposer` composes a
table for SVG and for a terminal; a terminal's marks ("! " against the machine, "·" empty) are measured as written.
SVG tables are the same, byte for byte; text and cells renders of tables change once, and the goldens are re-pinned.
The grid's colours are string views, so no cell builds a string to say its colour.
