The layout module's architecture is written as requirements held by rules (DS-392): the layout depends on nothing
of render or the scene, as it already depended on nothing of the model, and whoever places reaches an engine only
through IEngine and a router only through IRouter, from the factories that name them: neither the scene nor render
holds an engine, a router or a phase of the layered engine. A new definition, `engine-part(x)`, names them. Nothing
a design writes changes.
