# Logic

The logical perspective says what must always hold. Structure and behaviour describe the
system; logic judges it. A rule is a sentence about the design that DriftSpec evaluates on
every check, and a state machine says in which order the operations of a class may be
called.

## A rule

A rule says what it speaks about (`for`), and what must hold for each of them (`holds`) or
must never happen (`never`). This one: no class of the store depends on a class of the web.

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

The body is a list of literals joined by commas. A literal is a relation of the model
(`m uses x`), a kind (`x is class`), containment (`x in shop@web`) or a comparison. A bare
name is a variable; something of the design is written with its whole identity.

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
says, the requirement it answers to, and whether the model satisfies it.

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
nothing takes `Shipped` anywhere on `cancel`.

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
