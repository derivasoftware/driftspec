# Behaviour

The functional perspective says what happens. It stands on the structure: a behaviour is
always the behaviour of something already declared (a method, a function, an operation of
an interface), and every message it sends has to be one the receiver offers.

## A sequence

A sequence tells one operation step by step. `self -> stock.has(item) -> ok` reads: I ask
my `stock` for `has(item)` and call the answer `ok`.

```ds
package shop {
  module store {
    class Stock {
      + has(item: std::string) -> bool
    }

    class Cart {
      - stock: Stock
      + add(item: std::string) -> bool
    }

    sequence Cart::add {
      self -> stock.has(item) -> ok
      return ok
    }
  }
}

diagram SEQ_Add {
  kind: sequence
  include: shop@store::Cart::add
}
```

## Messages are checked

A message is held to the classes. One sent to an operation the receiver does not have is a
finding, at the line where it is written.

```ds wrong
package shop {
  module store {
    class Stock {
      + has(item: std::string) -> bool
    }

    class Cart {
      - stock: Stock
      + add(item: std::string) -> bool
    }

    sequence Cart::add {
      self -> stock.reserve(item) -> ok
      return ok
    }
  }
}

diagram SEQ_Add {
  kind: sequence
  include: shop@store::Cart::add
}
```

## Branches and loops

`opt` is a step that may happen, `alt` chooses between two, `loop` repeats. What decides is
written as plain words after the keyword.

```ds
package shop {
  module store {
    class Line {
      + subtotal() -> int
    }

    class Cart {
      - owns many lines: std::vector<Line>
      + total() -> int
    }

    sequence Cart::total {
      local line: Line
      loop each line {
        self -> line.subtotal() -> part
      }
      return sum
    }
  }
}

diagram SEQ_Total {
  kind: sequence
  include: shop@store::Cart::total
}
```

## Scenarios

A branch may carry a name after a slash, and that name is a scenario: one way the operation
can go. A diagram can ask for the whole behaviour or for one scenario alone.

```ds
package shop {
  module store {
    class Stock {
      + has(item: std::string) -> bool
      + reserve(item: std::string) -> void
    }

    class Cart {
      - stock: Stock
      + add(item: std::string) -> bool
    }

    sequence Cart::add {
      self -> stock.has(item) -> ok
      opt the item is out of stock / sold-out {
        return false
      }
      self -> stock.reserve(item)
      return true
    }
  }
}

diagram SEQ_Add {
  kind: sequence
  include: shop@store::Cart::add
}

diagram SEQ_SoldOut {
  kind: sequence
  include: shop@store::Cart::add / sold-out
}
```

## One call inside another

When a sequence calls an operation that has a sequence of its own, the diagram can follow it.
`depth:` says how far: `1` stays at the surface, `*` goes all the way down.

```ds
package shop {
  module store {
    class Card {
      + pay(amount: int) -> bool
    }

    class Line {
      + subtotal() -> int
    }

    class Cart {
      - owns many lines: std::vector<Line>
      + total() -> int
      + checkout(card: Card) -> bool
    }

    sequence Cart::total {
      local line: Line
      loop each line {
        self -> line.subtotal() -> part
      }
      return sum
    }

    sequence Cart::checkout {
      -> total() -> amount
      self -> card.pay(amount) -> paid
      return paid
    }
  }
}

diagram SEQ_Checkout {
  kind: sequence
  include: shop@store::Cart::checkout
  depth: *
}
```

## Failures

An operation says what it may fail with (`fails SoldOut`). A sequence ends a path with
`fail`, and whoever calls it handles the failure with `attempt … on`.

```ds
package shop {
  module store {
    class Stock {
      + reserve(item: std::string) -> void fails SoldOut
    }

    class Cart {
      - stock: Stock
      + add(item: std::string) -> bool
    }

    sequence Stock::reserve {
      opt nothing is left / empty {
        fail SoldOut
      }
    }

    sequence Cart::add {
      attempt {
        self -> stock.reserve(item)
      } on SoldOut {
        return false
      }
      return true
    }
  }
}

diagram SEQ_Add {
  kind: sequence
  include: shop@store::Cart::add
  depth: *
}
```

## The behaviour of an interface

An operation of an interface has a sequence too: what the package does when it is asked from
outside. Here the command line builds a cart and charges it.

```ds
package shop {
  interface shell {
    kind: cli
    operation buy(item: string) -> int {
      command: shop buy <item>
      exit 0: default
    }
  }

  sequence shell::buy {
    local cart: store::Cart
    local card: store::Card
    self -> cart.checkout(card) -> paid
    return 0
  }

  module store {
    class Card {
      + pay(amount: int) -> bool
    }

    class Cart {
      + checkout(card: Card) -> bool
    }

    sequence Cart::checkout {
      self -> card.pay(amount) -> paid
      return paid
    }
  }
}

diagram SEQ_Buy {
  kind: sequence
  include: shop@shell::buy
  depth: *
}
```

## A tour

A tour plays a behaviour over the boxes it crosses: the map on one side, the sequence on the
other, and each message lit in both. In the playground it has a play button.

```ds
package shop {
  interface shell {
    kind: cli
    operation buy(item: string) -> int {
      command: shop buy <item>
      exit 0: default
    }
  }

  sequence shell::buy {
    local cart: store::Cart
    local card: store::Card
    self -> cart.checkout(card) -> paid
    return 0
  }

  module store {
    class Card {
      + pay(amount: int) -> bool
    }

    class Cart {
      + checkout(card: Card) -> bool
    }

    sequence Cart::checkout {
      self -> card.pay(amount) -> paid
      return paid
    }
  }
}

diagram TOUR_Buy {
  kind: tour
  include: shop@shell::buy
  depth: *
}
```
