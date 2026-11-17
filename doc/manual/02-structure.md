# Structure

The structural perspective says what there is: the boxes of the system and what each one
holds, from the packages you would draw on a whiteboard down to the attributes of a class.
It is written once, and every diagram is a question asked of it.

## Packages, modules and identity

A package is a box of the system; a module is a box of code inside it; a class lives in a
module. Each has one identity, written the same way everywhere: `shop@store::Cart` is the
package, `@`, the module, `::`, the name.

```ds
package shop {
  module store {
    class Cart {
      + total() -> int
    }
  }
  module billing {
    class Invoice {
      + refs cart: store::Cart
    }
  }
}

diagram CL_Shop {
  kind: class
  include: shop@*
}
```

A workspace is a folder of `.ds` files read as one. A package or a module may be reopened in
any file, so the files are yours to organise: one per class, one per module, or one for
everything.

## The high-level design

From far away you want the boxes, not the classes. A diagram closes them with `close:` and
shows each one with what it holds; the arrows between closed boxes are the relations of
their insides, lifted.

```ds
package shop {
  module store {
    class Cart {
      + total() -> int
    }
    class Line {
      + refs cart: Cart
    }
  }
  module billing {
    class Invoice {
      + refs cart: store::Cart
      + print() -> string
    }
  }
}

diagram HLD_Shop {
  kind: class
  include: shop@*
  close: module
}
```

`open: shop@store` does the opposite: that box stays open and the rest close. Between two
closed boxes a solid arrow is a structural bond (one owns or refers to something of the
other) and a dashed one is a use.

## Interfaces

An interface is how a package is reached from outside: a command line, an HTTP API, a
library. It lists its operations, and each may say how it ends.

```ds
package shop {
  interface shell {
    kind: cli
    operation buy(item: string) -> int {
      command: shop buy <item>
      exit 0: default
      exit 2: declined
    }
  }

  sequence shell::buy {
    return 0
  }

  sequence shell::buy / declined {
    return 2
  }
}

diagram IF_Shell {
  kind: interface
  include: shop@shell
}
```

A box that goes through an interface of another says so with `@uses`, and the arrow between
the two tells which interface it is when you point at it.

## Classes and their members

A member starts with its visibility: `+` public, `#` protected, `-` private. An attribute is
a name and a type; a method is a name, its parameters and what it returns. A method may be
`static`, `abstract` or `const`, and an attribute may carry a default.

```ds
package shop {
  module store {
    class Cart {
      + total() -> int
      + add(price: int) -> none
      # discount(percent: int) -> int
      - count: int = 0
      + static empty() -> Cart
    }
  }
}

diagram CL_Store {
  kind: class
  include: shop@store::*
}
```

## Values and enumerations

A `dataclass` is a value: two with equal fields are the same. An `enum` is a closed list of
names.

```ds
package shop {
  module store {
    enum Status { Open, Paid, Sent }

    dataclass Money {
      + cents: int
      + currency: string
    }

    class Cart {
      + status: Status
      + total() -> Money
    }
  }
}

diagram CL_Store {
  kind: class
  include: shop@store::*
}
```

## Relations

You never draw an arrow. A relation comes from what a class says of its attributes and its
methods:

- `owns` (the default): the class creates and destroys the part. Drawn as a composition.
- `refs`: the part lives somewhere else; the class only knows it. Drawn as an aggregation.
- `shares`: the lifetime is shared with other holders.
- A type named in a parameter or a return is a use, drawn dashed.

How many comes from the type: `Line` is one, `optional<Line>` is none or one, `list<Line>`
is many.

```ds
package shop {
  module store {
    class Line {
      + price: int
    }

    class Catalogue {
      + priceOf(item: string) -> int
    }

    class Coupon {
      + percent: int
    }

    class Cart {
      - owns lines: list<Line>
      - refs catalogue: Catalogue
      + apply(coupon: Coupon) -> none
      + total() -> int
    }
  }
}

diagram CL_Store {
  kind: class
  include: shop@store::*
  show: ownership, multiplicity
}
```

## Inheritance

A class names its base after a colon. An interface is never written as such: a class whose
methods are all abstract and that keeps no data is one, and the drawing says so.

```ds
package shop {
  module billing {
    class Payment {
      + abstract pay(amount: int) -> bool
    }

    class Card : Payment {
      + pay(amount: int) -> bool
    }

    class Transfer : Payment {
      + pay(amount: int) -> bool
    }
  }
}

diagram CL_Billing {
  kind: class
  include: shop@billing::*
}
```

## Functions

Not everything is a class. A `function` is an operation of the module itself, and it is
drawn in the module's own box.

```ds
package shop {
  module billing {
    class Invoice {
      + number: int
    }

    function invoice(amount: int) -> Invoice
    function today() -> string
  }
}

diagram CL_Billing {
  kind: class
  include: shop@billing::*
}
```

## A name nobody declares

Every name the design uses has to exist. One that does not is drawn as a ghost and reported,
with the line where it is written.

```ds wrong
package shop {
  module store {
    class Cart {
      - owns lines: list<Line>
    }
  }
}

diagram CL_Store {
  kind: class
  include: shop@store::*
}
```

`W-REF-UNDECLARED` says that `Line` is used and never declared. Declare it, or fix the name.

## Choosing what a diagram shows

A diagram takes what `include:` names and leaves out what `exclude:` names. `hide:` drops a
compartment and `show:` adds an extra. The same classes can be asked for in as many diagrams
as you need.

```ds
package shop {
  module store {
    class Line {
      + price: int
    }

    class Cart {
      - owns lines: list<Line>
      + total() -> int
    }

    class Receipt {
      + refs cart: Cart
      + print() -> string
    }
  }
}

diagram CL_Carts {
  kind: class
  include: shop@store::*
  exclude: shop@store::Receipt
  hide: methods
}
```
