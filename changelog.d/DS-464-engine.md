A sequence is placed by one engine of the layout (DS-464, step 1): `layout::StackedEngine`, behind
`ISequenceEngine` and named by `EngineFactory::sequence`, places a `SequenceGraph` (lanes, messages by row, frames)
into a `SequencePlacement` with a `SequenceSpacing` made from the sink's measures: proportions written once, across in
characters and down in lines, so the SVG's 8 px, 18 px and its small text's 7 px give exactly the pixels it always
had. render projects the IR into the graph (`SequenceProjection`) and the SVG composes its sequences through the
engine. The theme's measures say a character of small text. Every drawing is the same, byte for byte; text sequences
move to the engine in the next step.
