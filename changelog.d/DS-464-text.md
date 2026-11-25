Text sequences are placed by the layout's sequence engine too (DS-464, step 2), as the Owner chose: one
`SequenceComposer` composes a sequence for SVG and for a terminal, the same proportions in pixels or in cells, so a
terminal's sequences now space their lanes, rows and frames as the SVG does. A sink states the least room its marks
need (`layout::SequenceLeast`, from `render::Metrics`): a terminal's boxed head and its loop of a message to its own
lane take three rows, a head's label a border and a space each side, a frame's edge a row of its own. Text and cells
renders of sequences and tours change once, and nothing else does; the goldens are re-pinned.
