The SVG drawing keeps its helpers as members (DS-371): what drew nodes, edges, markers, members, sequences, tables
and the parts of a drawing are private members of `Svg`, the marker table and the sequence's script its private
constants; what an edge is drawn as and a sequence's lane are the dataclasses `EdgeMarkup` and `Lane`. render's design
names no free function any more. Every drawing is the same, byte for byte.
