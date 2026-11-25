A box diagram is composed below its drawing (DS-400): `render::Figure` holds the document as it is drawn and where
each node and route lands, made by a composer of its kind and technology behind `IComposer` (`BoxesSvgComposer`,
`BoxesTextComposer`), which `ComposerFactory` names. `Svg::draw` and `Text::draw` draw a figure and place nothing;
the svg, text, cells and layout outputs reach the layout only through their composer. Every drawing is the same, byte
for byte.
