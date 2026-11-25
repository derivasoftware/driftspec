# Walkthroughs

A walkthrough takes a reader through the design one step at a time. Each step shows one of
the workspace's diagrams, lights what it is about, and says it in a sentence. It names
everything by identity, so the check tells a step that still names what was renamed or
removed: a walkthrough cannot drift away from the design the way prose does.

## A walkthrough

Two diagrams of a small shop, and a walk through them in two steps.

```ds
package shop {
  module desk {
    class Desk {
      - counter: Counter
      + sell(item: std::string) -> int
    }

    class Counter {
      + take(item: std::string) -> bool
    }

    sequence Desk::sell {
      self -> counter.take(item) -> held
      return 0
    }
  }
}

diagram CL_Desk {
  kind: class
  include: shop@*
}

diagram SEQ_Sell {
  kind: sequence
  include: shop@desk::Desk::sell
}

diagram WALK_Sell {
  kind: walkthrough
  title: How the desk sells an item
  step Who is involved {
    show: CL_Desk
    focus: shop@desk::Desk
    highlight: shop@desk::Counter
    at: shop@desk::Desk
    text: The desk sells; it holds the counter it takes items from.
  }
  step The sale {
    show: SEQ_Sell
    focus: shop@desk::Desk::sell
    highlight: shop@desk::Counter::take
    at: shop@desk::Counter::take
    text: To sell an item the desk asks the counter to take it.
  }
}
```

The steps are numbered in the order they are written: the first step of `WALK_Sell` is
`diagram:WALK_Sell::1`.

## What a step says

`show:` names the diagram the step shows. A sequence diagram may be shown along one of its
scenarios: `show: SEQ_Buy / sold-out`.

`focus:` is what the step is about, and `highlight:` what stands beside it. Both take one
identity or several, separated by commas. A member is lit in the box that holds it: an
operation in its interface, a method in its class.

`at:` is where the step's callout points. It is one of the identities the step focuses or
highlights.

`text:` is the sentence the reader is told at that step.

`title:` on the walkthrough says in one line what it is about.

## What the check tells

Every step is held to the design. A step that names a diagram or an entity nothing declares
is W-WALK-UNDECLARED. One that lights what the diagram it shows does not draw is
W-WALK-NOT-SHOWN: what a diagram draws is what its own filter takes. A callout that points
at what the step does not light is W-WALK-AT, a scenario the behaviour does not have is
W-SCENARIO-UNDECLARED, and a walkthrough with no step, or a step with nothing to show or
nothing to say, is W-WALK-EMPTY. Each is a warning: a walkthrough documents the design and
never breaks it.

```ds wrong
package shop {
  module desk {
    class Desk {
      + sell(item: std::string) -> int
    }
  }
}

diagram CL_Desk {
  kind: class
  include: shop@*
}

diagram WALK_Sell {
  kind: walkthrough
  step Who sells {
    show: CL_Desk
    focus: shop@desk::Till
    text: The till sells.
  }
}
```

## Reading one

```bash
ds query design walkthroughs              # the walkthroughs, each with its title
ds query design walkthroughs WALK_Sell    # its steps in order, and what each shows
ds export design                          # walkthroughs, by identity, in the export
ds render design -f json                  # and in the scene the viewer reads
```

The example `examples/shop` holds one, `WALK_Buy`, from the shop's promise to its classes.
