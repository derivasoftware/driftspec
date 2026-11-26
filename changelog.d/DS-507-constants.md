The rule evaluator looks up what a rule's constants stand for once, when it first solves each body, never at every step
(DS-507, second part). Each constant term's identity and the definition each call applies are resolved in the facts
once and kept with the body's compiled slots (`CompiledBody`). `ds check` spends a further 10% fewer instructions on
DriftSpec's own design and on the playground's tour (28% fewer than before DS-507), and the same on a design with no
rules; every finding, breach and table is as before, byte for byte.
