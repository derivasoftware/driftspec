# Logic

The logical perspective says what must always hold. Structure and behaviour describe the
system; logic judges it. A rule is a sentence about the design that DriftSpec evaluates on
every check, and a state machine says in which order the operations of a class may be
called.

## A rule

A rule says what it speaks about (`for`), and what must hold for each of them (`holds`) or
must never happen (`never`). This one: no method of the store names a class of the web.

```ds
package shop {
  requirement REQ-00001-1: The store shall not depend on the web

  module web {
    class Page {
      + show() -> void
    }
  }

  module store {
    class Order {
      + total() -> int
    }

    rule store-knows-no-web {
      title: No class of the store uses the web
      @requirement REQ-00001-1
      for class c in shop@store
      never m in c, m uses x, x in shop@web
    }
  }
}

diagram CL_Shop {
  kind: class
  include: shop@*
}
```

The body is a list of literals joined by commas, all of which must hold at once. A literal is
a relation of the model (`m uses x`), a kind (`x is class`), containment (`x in shop@web`,
at any depth) or a comparison. A bare name is a variable; something of the design is written
with its whole identity. A variable that `for` does not bind means "some": `m in c, m uses x`
reads "some member of c uses some x".

A rule answers to a requirement with `@requirement`, as a class does: it is how the
requirement is made true in that box. A rule that answers to none is an open item of coverage.

## A rule that is broken

When the design breaks a rule, the finding names the rule and what breaks it. Here an order
has started to draw itself on a page.

```ds wrong
package shop {
  requirement REQ-00001-1: The store shall not depend on the web

  module web {
    class Page {
      + show() -> void
    }
  }

  module store {
    class Order {
      + total() -> int
      + draw(page: web::Page) -> void
    }

    rule store-knows-no-web {
      title: No class of the store uses the web
      @requirement REQ-00001-1
      for class c in shop@store
      never m in c, m uses x, x in shop@web
    }
  }
}

diagram CL_Shop {
  kind: class
  include: shop@*
}
```

## What a relation sees

Each relation starts from some kinds of entity and not from others, and a rule reads exactly
what the model records. A class relates to other classes through its members: an attribute
owns, refers to or shares what it holds (`owns`, `refs`, `shares`), and `uses` is a type that
a signature or a behaviour names. So the rule above does not see a store that keeps a page in
an attribute: `- refs page: web::Page` is a `refs`, and the rule still holds.

| Relation | From | To |
|---|---|---|
| `in` | anything | the box that holds it, at any depth |
| `owns`, `refs`, `shares` | an attribute | the class or enum it holds |
| `uses` | a method, an operation, a sequence | a type its signature or its steps name |
| `inherits`, `realises` | a class | the class it derives from; the interface it carries out |
| `satisfies`, `constrained-by` | a piece of the design | the requirement it makes true; the constraint it obeys |
| `verifies`, `refines` | a verification; a requirement | what it verifies; the requirement it details |

`ds manual <kind>` lists every relation from and to that kind, and `ds query <workspace>
relations <id>` shows the ones an entity takes part in.

## Saying "depends" once: a definition

When the same idea appears in several rules, name it once. A definition is a relation made
of others, one `if` line per case in which it holds; any one case is enough. This one says
what depending is for this shop, through any member, and two rules use it.

```ds
package shop {
  requirement REQ-00001-1: The store shall not depend on the web

  definition depends(c, x) {
    title: c depends on x through one of its members
    if m in c, m uses x
    if m in c, m owns x
    if m in c, m refs x
    if m in c, m shares x
  }

  module web {
    class Page {
      + show() -> void
    }
  }

  module store {
    class Order {
      + total() -> int
    }
    class Basket {
      - refs order: Order
    }
  }

  rule store-knows-no-web {
    title: No class of the store depends on the web
    @requirement REQ-00001-1
    for class c in shop@store
    never depends(c, x), x in shop@web
  }

  rule web-knows-no-basket {
    title: No class of the web depends on the basket
    @requirement REQ-00001-1
    for class c in shop@web
    never depends(c, shop@store::Basket)
  }
}
```

A definition written in a package is seen by the rules of that package and of its modules. It
may apply other definitions, and itself: a definition that reaches through a chain of `uses`
is written with one case for one step and one for one step more.

## When a rule speaks of nothing

A rule that names a relation, a kind, a box or a definition the design does not have would
hold forever and prove nothing, so it is a finding of its own: `E-RULE-UNKNOWN` for a name
nothing declares, and `W-RULE-NEVER-MATCHES` for a literal that can never be true. Here `use`
is not a relation; the relation is `uses`.

```ds wrong
package shop {
  requirement REQ-00001-1: The store shall not depend on the web

  module web {
    class Page {
      + show() -> void
    }
  }

  module store {
    class Order {
      + total() -> int
    }

    rule store-knows-no-web {
      title: No class of the store uses the web
      @requirement REQ-00001-1
      for class c in shop@store
      never m in c, m use x, x in shop@web
    }
  }
}
```

## Filtering and reading fields

`where` keeps only some of what `for` ranges over, and a comparison may read a field of what
a variable is bound to. This rule asks that every approved requirement be satisfied by something.

```ds
package shop {
  requirement REQ-00001-1 {
    title: The cart shall total the price of its lines
    status: approved
  }
  requirement REQ-00002-1 {
    title: The cart shall remember its customer
    status: draft
  }

  module store {
    class Cart {
      + total() -> int {
        @requirement REQ-00001-1
      }
    }
  }

  rule approved-are-kept {
    title: Every approved requirement is satisfied by something
    @requirement REQ-00001-1
    for requirement r in shop
    where r.status = "approved"
    holds x satisfies r
  }
}

diagram REQ_Shop {
  kind: requirement
  include: shop@*
}
```

## What an entity is, and its name against a pattern

A rule reads what an entity is as a function of the variable bound to it: `name(x)` is its name as written,
`id(x)` its identity and `kind(x)` the name of its kind. They read what `x` is bound to and never bind it, and no
definition may take those names. The dot stays for a field the design writes, as `r.status`.

`matches` holds when a value contains a match of a pattern, written as a string in ECMAScript regular expression
syntax; `^` and `$` anchor it. Each pattern is compiled once, when its rule is checked, and a pattern that does not
compile is a finding at the rule (E-RULE-PATTERN). A member's flag reads as a kind does: `m is abstract`.

These rules ask that every interface be named I and a capital, and every class with an abstract member A and a
capital.

```ds
package shop {
  requirement REQ-00001-1 {
    title: The shop shall name its boxes by their role
  }

  module store {
    interface IPricing {
      operation price(item: string) -> int
    }
    class APricing {
      + abstract price(item: string) -> int
      + currency() -> string
    }
  }

  rule interfaces-are-named-i {
    title: An interface is named I and a capital
    @requirement REQ-00001-1
    for interface i in shop
    holds name(i) matches "^I[A-Z]"
  }

  rule abstract-classes-are-named-a {
    title: A class with an abstract member is named A and a capital
    @requirement REQ-00001-1
    for class c in shop
    where m in c, m is abstract
    holds name(c) matches "^A[A-Z]"
  }
}
```

## The rules as a table

A table lists whatever a rule may range over. This one lists the rules of the shop: what each
says, the requirement it answers to, and whether the model satisfies it. `r.holds` in a column
is that verdict, `holds` or `broken`; `r.never` is the text of a `never`. A table and its
columns are written as in the intent chapter.

```ds
package shop {
  requirement REQ-00001-1: The store shall not depend on the web

  module web {
    class Page {
      + show() -> void
    }
  }

  module store {
    class Order {
      + total() -> int
    }

    rule store-knows-no-web {
      title: No class of the store uses the web
      @requirement REQ-00001-1
      for class c in shop@store
      never m in c, m uses x, x in shop@web
    }

    rule store-refers-to-no-web {
      title: No attribute of the store refers to the web
      @requirement REQ-00001-1
      for attribute a in shop@store
      never a refs t, t in shop@web
    }
  }
}

diagram TB_Rules {
  kind: table
  for rule r in shop
  column rule: r.title
  column requirement: x where r satisfies x
  column model: r.holds
}
```

## A state machine

A state machine is the life of a class: the states it goes through and the operation that
takes it from one to the next.

```ds
package shop {
  module store {
    class Order {
      + pay() -> void
      + ship() -> void
      + cancel() -> void
    }

    state Order {
      initial Open
      final Shipped, Cancelled
      Open -> Paid on pay
      Paid -> Shipped on ship
      Open -> Cancelled on cancel
      Paid -> Cancelled on cancel
    }
  }
}

diagram ST_Order {
  kind: state
  include: shop@store::Order
}
```

`initial` says where the machine starts, and `final` the states it ends in. The drawing shows
the start as a filled dot with an arrow to the initial state, and each final state with an
arrow to a bullseye. A transition out of a final state is reported (W-STATE-LEAVES-FINAL): the
machine cannot both end there and go on. A state that nothing leaves is not taken as final; a
machine written in part says only what it says.

## The machine as a table

The same machine as a table: the operations down, the states across, and in each cell where
the operation takes the state. An empty cell is a pair the machine says nothing about.

```ds
package shop {
  module store {
    class Order {
      + pay() -> void
      + ship() -> void
      + cancel() -> void
    }

    state Order {
      initial Open
      Open -> Paid on pay
      Paid -> Shipped on ship
      Open -> Cancelled on cancel
      Paid -> Cancelled on cancel
    }
  }
}

diagram TB_Order {
  kind: table
  include: shop@store::Order
}
```

## The machine holds the behaviour

A state machine is checked against the sequences. A behaviour that calls an operation when
the machine does not allow it is a finding: here an order is shipped and then cancelled, and
nothing takes `Shipped` anywhere on `cancel`. The check follows the calls one sequence makes on
one attribute or local, in order, starting from any state the machine has; it does not assume
the machine starts at `initial`, and it does not carry a state into a call that has a sequence
of its own.

```ds wrong
package shop {
  module store {
    class Order {
      + pay() -> void
      + ship() -> void
      + cancel() -> void
    }

    state Order {
      initial Open
      Open -> Paid on pay
      Paid -> Shipped on ship
      Open -> Cancelled on cancel
      Paid -> Cancelled on cancel
    }

    class Desk {
      + send(order: Order) -> void
    }

    sequence Desk::send {
      self -> order.pay()
      self -> order.ship()
      self -> order.cancel()
    }
  }
}

diagram TB_Order {
  kind: table
  include: shop@store::Order
}
```
