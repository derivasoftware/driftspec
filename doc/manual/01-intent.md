# Intent

Before saying how a system is built, a design says what it is for. DriftSpec writes that
down as four kinds of artefact: use cases, requirements, constraints and verifications. They
are text in the same files as the rest of the design, each with an identity, and everything
else points at them.

## A requirement

A requirement is one sentence the system promises to keep. It has an identity (`REQ-00001-1`:
a number and a revision), a title and a status.

```ds
package shop {
  requirement REQ-00001-1 {
    title: The cart shall total the price of its lines
    status: draft
    rationale:
      A customer sees the total before paying.
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

A short one fits on a line: `requirement REQ-00002-1: The shop shall take an order`.

## From the system down

A requirement written inside another refines it. The ones at the top say what the system
does; the ones below say what each part does to make that true.

```ds
package shop {
  requirement SREQ-00001-1 {
    title: The shop shall sell an item from the command line
    status: draft

    requirement REQ-00001-1: The cart shall total the price of its lines
    requirement REQ-00002-1: The cart shall charge its total to a card
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

A requirement written somewhere else says the same with `@refines SREQ-00001-1`.

## Who keeps a requirement

A requirement is kept by the part of the design that names it with `@requirement`. One that
nothing names is a promise with no one behind it, and DriftSpec says so.

```ds
package shop {
  requirement REQ-00001-1: The cart shall total the price of its lines
  requirement REQ-00002-1: The cart shall charge its total to a card

  module store {
    class Card {
      + pay(amount: int) -> bool
    }

    class Cart {
      + total() -> int {
        @requirement REQ-00001-1
      }
      + checkout(card: Card) -> bool {
        @requirement REQ-00002-1
      }
    }
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

## A verification

A verification says how a requirement is proved: by a test, a review, an analysis or an
inspection. Nested in the requirement it proves, or anywhere else with `@requirement`.

```ds
package shop {
  requirement REQ-00001-1 {
    title: The cart shall total the price of its lines
    status: draft

    verification VER-00001-1 {
      title: Unit tests confirm the cart totals its lines
      method: test
      tests:
        tests/test_cart.cpp::TestCart.Totals
    }
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

## A constraint

A constraint is a requirement about how things are made, not about what they do: a rule
every instance of a category obeys. What obeys it says so with `@constraint`.

```ds
package shop {
  constraint CST-00001-1: Every price shall be kept in cents

  module store {
    dataclass Money {
      @constraint CST-00001-1
      + cents: int
    }
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

## A use case

A use case is a contract seen from outside: what goes in, what comes out, and every way it
can end. It answers to the system requirements, and an operation of an interface carries it
out (see Structure).

```ds
package shop {
  requirement SREQ-00001-1: The shop shall sell an item from the command line

  usecase UC-00001-1 buy(item: string) -> int {
    title: Buy an item
    status: draft
    @requirement SREQ-00001-1
    outcome default: the number of the order
    outcome declined: nothing is bought
    outcome sold-out: the item is not in stock
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

## All of them as a table

A table lists whatever you ask for, a row each, with a column per question. This one lists
the requirements of the shop with who keeps each and what proves it; an empty cell is a
requirement nobody keeps or nobody proves.

```ds
package shop {
  requirement REQ-00001-1 {
    title: The cart shall total the price of its lines
    status: draft

    verification VER-00001-1 {
      title: Unit tests confirm the cart totals its lines
      method: test
      tests:
        tests/test_cart.cpp::TestCart.Totals
    }
  }
  requirement REQ-00002-1: The cart shall charge its total to a card

  module store {
    class Cart {
      + total() -> int {
        @requirement REQ-00001-1
      }
    }
  }
}

diagram TB_Requirements {
  kind: table
  for requirement r in shop
  column requirement: r.id
  column says: r.title
  column kept: x where x satisfies r
  column proved: v where v verifies r
}
```
