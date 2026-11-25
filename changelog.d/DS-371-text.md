The terminal drawing keeps its helpers as members (DS-371): what drew boxes, containers, edges, members, sequences,
tables and the parts of a drawing are private members of `Text`; the character grid is the class `Grid`, with its
`frame` and `clear`, over the dataclasses `Cell` and `SeqLine`. A node's badge letter and whether a section holds
members are `IrNode::badgeOf` and `IrNode::holdsMembers`. Every drawing is the same, byte for byte.
