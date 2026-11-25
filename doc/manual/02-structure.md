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
      + print() -> std::string
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

A box that goes through an interface of another says so with `@consumes`, and the arrow between
the two tells which interface it is when you point at it.

Inside a package an interface is also a contract in code: a module offers it, a class carries
it out with `@interface`, and others hold it without knowing the class. Its operation's sequence
says which class answers; until it has one, the operation is an open item of coverage.

```ds
package shop {
  module store {
    interface IPricing {
      operation price(item: string) -> int
    }

    class ListPricing {
      @interface IPricing
      + price(item: string) -> int
    }

    class Cart {
      - refs pricing: IPricing
      + total() -> int
    }

    sequence IPricing::price {
      local pricing: ListPricing
      self -> pricing.price(item) -> cents
      return cents
    }
  }
}

diagram CL_Pricing {
  kind: class
  include: shop@store::*
}
```

## Classes and their members

A member starts with its visibility: `+` public, `#` protected, `-` private. An attribute is
a name and a type; a method is a name, its parameters and what it returns. A method may be
`static`, `abstract` or `const`, and an attribute may carry a default.

```ds
package shop {
  module store {
    class Cart {
      + total() -> int
      + add(price: int) -> void
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

## One name, several operations

A class, an interface or a module may declare two operations of one name with different
parameters. Each is then named by its parameter types as written: `Factory::of(Scene)`,
`Factory::of(Report, int)`, `Factory::of()`. An operation alone with its name keeps its bare
name. A behaviour names the one it is of by its types.

```ds
package shop {
  module store {
    class Scene { }
    class Report { }

    class Factory {
      + static of(document: Scene) -> int
      + static of(report: Report, depth: int) -> int
    }

    class Desk {
      - factory: Factory
      + open(scene: Scene) -> int
    }

    sequence Factory::of(Scene) {
      return 1
    }

    sequence Desk::open {
      self -> factory.of(scene) -> made
      return made
    }
  }
}

diagram CL_Store {
  kind: class
  include: shop@store::*
}
```

A call matches only the overloads it could be a call of: `factory.of(scene)` has one
argument, so it calls `of(Scene)`. A call that two still match, or a behaviour that names
`Factory::of` bare, is `W-NAME-AMBIGUOUS` and names them: write the parameter types.

## Values and enumerations

A `dataclass` is a value: two with equal fields are the same. An `enum` is a closed list of
names.

```ds
package shop {
  module store {
    enum Status { Open, Paid, Sent }

    dataclass Money {
      + cents: int
      + currency: std::string
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

A type is written as your code writes it: `std::vector<Line>`, `list[Line]`, `Line | None`,
`const Catalogue&`. DriftSpec knows no language: it reads the names in the type, and each name the
design declares is related; any other name (`std::string`, `int`, a library's class) is text.
A dot qualifies a name as `::` does: `store.Card` in a Python type is `store::Card`.

How many is written in front of the name, never read from the type: `many` is any number,
`optional` is none or one, and nothing is exactly one. They go with `refs` and `shares`:
`- refs many watchers: std::set<Watcher*>`. Two ownerships, or two multiplicities, on one attribute
say two things at once (`E-MODIFIER-CONFLICT`).

```ds
package shop {
  module store {
    class Line {
      + price: int
    }

    class Catalogue {
      + priceOf(item: std::string) -> int
    }

    class Coupon {
      + percent: int
    }

    class Cart {
      - owns many lines: std::vector<Line>
      - refs catalogue: Catalogue
      + apply(coupon: Coupon) -> void
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

A class names its base after a colon, written as the code writes it, template arguments and all:
`class SceneJson : IFormat<Scene>`, `class IntBox : Box[int]`. Its first name is what the class
inherits, when the design declares it; a base nobody designs (`Exception`) is text and draws no
arrow. A template declares its parameters after its name, a class (`class Box<T>`) or a method
(`+ each<F>(f: F) -> void`) alike. Whether a class is abstract is never written: a class
with an abstract method is abstract, and the drawing says so. A class whose methods are all
abstract is still a class, and what derives from it inherits from it. An interface is something
else: the contract a box offers, written with `interface`.

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
    function today() -> std::string
  }
}

diagram CL_Billing {
  kind: class
  include: shop@billing::*
}
```

## A name nobody declares

You design what you want to say, and nothing else. A name the design does not declare, in a type,
a base or a call, is text: `std::string`, `Exception`, a library's `Gateway`. It relates nothing,
it is not drawn and it is no finding, so what lives outside your design costs nothing to name.
A bare name that no scope declares is the one class of that name in the package, as your code
imports it from a sibling module. With two of that name it relates nothing and
`W-NAME-AMBIGUOUS` names both; qualify it: `store::Card`.

```ds
package shop {
  module store {
    class Line {
      + note: std::string
    }

    class Error : Exception {
      + line: Line
    }
  }

  module billing {
    class Invoice {
      - refs many lines: std::vector<Line>
    }
  }
}

diagram CL_Shop {
  kind: class
  include: shop@*
}
```

A misspelt name is text too, and its arrow is missing from the drawing. argos, comparing the
design with the code, tells you where the code relates what the design does not.

```ds wrong
package shop {
  module store {
    class Card { }
  }

  module payment {
    class Card { }
  }

  module billing {
    class Invoice {
      - card: Card
    }
  }
}

diagram CL_Shop {
  kind: class
  include: shop@*
}
```

`W-NAME-AMBIGUOUS` says that `Card` in `Invoice` could be `shop@payment::Card` or
`shop@store::Card`. Write `store::Card` and the arrow is drawn.

## Two things at once

An attribute has one ownership and one multiplicity. Writing two of a kind is an error.

```ds wrong
package shop {
  module store {
    class Line {
      + price: int
    }

    class Cart {
      - refs shares many lines: std::vector<Line>
    }
  }
}

diagram CL_Store {
  kind: class
  include: shop@store::*
}
```

`E-MODIFIER-CONFLICT` says that `lines` is both `refs` and `shares`. Keep the one you mean.

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
      - owns many lines: std::vector<Line>
      + total() -> int
    }

    class Receipt {
      + refs cart: Cart
      + print() -> std::string
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
