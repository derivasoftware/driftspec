render's drawings place nothing, written as a requirement held by rules (DS-464, step 3): Svg and Text depend on no
composer, on neither render's box projection nor the sequence projection, and on nothing of the layout module, so they
reach what places a diagram only through IComposer, from ComposerFactory. Two new definitions, `draws(c)` and
`places(x)`, name them. Nothing a design writes changes.
