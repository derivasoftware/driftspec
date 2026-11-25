# Intent

Before saying how a system is built, a design says what it is for. DriftSpec writes that
down as four kinds of artefact: use cases, requirements, constraints and verifications. They
are text in the same files as the rest of the design, each with an identity, and everything
else points at them.

## How things link

Everything in a design points at the requirement it serves, and a link is always written inside
the thing that points: an `@` word and the id of what it points at. This small shop has every kind
of link, each marked with a comment; the drawing beside it shows them as arrows.

```ds
package shop {
  requirement SREQ-00001-1: The shop shall sell an item from the command line
  constraint CST-00001-1: Every price shall be kept in cents

  module store {
    requirement REQ-00001-1 {
      title: The cart shall total the price of its lines
      status: draft
      @refines SREQ-00001-1               // details the system requirement

      verification VER-00001-1 {          // nested: proves REQ-00001-1
        title: Unit tests confirm the cart totals its lines
        method: test
        tests:
          tests/test_cart.cpp::TestCart.Totals
      }
    }

    dataclass Money {
      @constraint CST-00001-1             // obeys the constraint
      + cents: int
    }

    class Cart {
      + total() -> Money {
        @requirement REQ-00001-1          // makes the requirement true
      }
    }
  }

  verification VER-00002-1 {
    title: A review confirms every price is kept in cents
    method: review
    @constraint CST-00001-1               // proves the constraint
    evidence:
      The fields of the store's money are reviewed on every change.
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

Read the links as five verbs:

- **makes true**: `@requirement` in the part of the design that does it, a package, a module, a
  class, a member, a sequence or a rule.
- **proves**: a verification nested in what it proves, or with `@requirement` or `@constraint`
  inside it.
- **details**: a requirement nested in another, or with `@refines` inside it.
- **obeys**: `@constraint` in what keeps the constraint.
- **carries out**: an interface operation names its use case after its signature
  (`operation buy(item: string) -> int : UC-00001-1`), and a class names the interface it
  implements (`@interface IPricing`).

`ds query <workspace> relations REQ-00001-1` shows every link of one artefact, both ways.

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

A requirement written somewhere else says the same with `@refines SREQ-00001-1`. Where a
requirement is written gives its level: in the package it speaks for the system, inside a
module for that module, and the requirement diagram shows the level beside its status. The
requirement of a part is best written in the part, beside what makes it true.

```ds
package shop {
  requirement SREQ-00001-1 {
    title: The shop shall sell an item from the command line
    status: draft
  }

  module store {
    requirement REQ-00001-1 {
      title: The cart shall total the price of its lines
      status: draft
      @refines SREQ-00001-1
    }

    class Cart {
      + total() -> int {
        @requirement REQ-00001-1
      }
    }
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

## What satisfies a requirement

A requirement is satisfied by the part of the design that names it with `@requirement`. One that
nothing names is a promise with no one behind it: once it is approved, it is an open item of
coverage until something satisfies it (see Using). A requirement that others refine is kept
when every one of them is.

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
inspection. Nested in the requirement it proves, or anywhere else with `@requirement`. A test
verification names its tests, one a line, as `file::Fixture.Test`; `level:` says whether they
are unit, integration or system tests, and a verification by analysis or review writes what it
rests on in `evidence:`. An approved requirement that nothing verifies is an open item of
coverage.

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

A constraint is a requirement that every instance of what it binds obeys: how things are made
(every price is kept in cents), or what must always or never happen. What obeys it says so with
`@constraint`, and a rule can check it. A verification proves a constraint the same way,
with `@constraint`.

```ds
package shop {
  constraint CST-00001-1: Every price shall be kept in cents

  verification VER-00001-1 {
    title: A review confirms every price of the store is kept in cents
    method: review
    @constraint CST-00001-1
    evidence:
      The fields of the store's money are reviewed on every change.
  }

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
out by naming it after its signature, `operation buy(item: string) -> int : UC-00001-1`; each
outcome is then a scenario of that operation with the same name (Behaviour shows the whole
chain).

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
requirement nothing satisfies or nothing proves.

`for` ranges over one kind. A column has a one-word name and either reads a field of the row
(`r.title`, `r.id`) or finds what the row is related to (`x where x satisfies r`): a bare `x`
writes the short names of all it finds in one cell, and `x.id` writes each full identity in a
row of its own. Constraints get a table of their own, with `x where x constrained-by c`.
A column also reads what the row's entity is, as a rule does: `name(r)`, `id(r)` or `kind(r)`.

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
  column satisfied: x where x satisfies r
  column proved: v where v verifies r
}
```
