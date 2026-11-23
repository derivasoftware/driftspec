#include <map>
#include <string>
#include <utility>

#include "Detail.hpp"
#include "ds/model/Metamodel.hpp"

// How each kind is written and a minimal workspace that declares one (DS-79): the metamodel's, which the
// kinds carry (KindSpec::syntax, KindSpec::example) and the manual prints. The manual reads them from here,
// never the other way round (DS-340).

namespace ds {
namespace model {

namespace detail {

const std::string kShopClass =
    "package shop {\n"
    "  module store {\n"
    "    dataclass Item {\n"
    "      + name: std::string\n"
    "      + cents: int\n"
    "    }\n"
    "    class Cart {\n"
    "      - owns many items: std::vector<Item>\n"
    "      + add(item: const Item&) -> void\n"
    "      + total() -> int\n"
    "    }\n"
    "  }\n"
    "}\n";

const std::string kShopInterface =
    "package shop {\n"
    "  interface shell {\n"
    "    kind: cli\n"
    "    operation buy(item: string) -> int {\n"
    "      command: shop buy <item>\n"
    "      exit 0: default\n"
    "    }\n"
    "  }\n"
    "\n"
    "  sequence shell::buy {\n"
    "    return 0\n"
    "  }\n"
    "}\n";

const std::string kShopSequence =
    "package shop {\n"
    "  module store {\n"
    "    class Stock {\n"
    "      + has(item: const std::string&) -> bool\n"
    "    }\n"
    "    class Cart {\n"
    "      - stock: Stock\n"
    "      + add(item: const std::string&) -> bool\n"
    "    }\n"
    "\n"
    "    sequence Cart::add {\n"
    "      self -> stock.has(item) -> ok\n"
    "      opt the item is out of stock / out {\n"
    "        return false\n"
    "      }\n"
    "      return ok\n"
    "    }\n"
    "  }\n"
    "}\n";

const std::string kShopUseCase =
    "package shop {\n"
    "  usecase UC-00001-1 buy(item: string) -> int {\n"
    "    title: Buy an item\n"
    "    status: draft\n"
    "    outcome default: the number of the order\n"
    "    outcome sold-out: nothing is bought\n"
    "  }\n"
    "\n"
    "  interface shell {\n"
    "    kind: cli\n"
    "    operation buy(item: string) -> int : UC-00001-1 {\n"
    "      command: shop buy <item>\n"
    "      exit 0: default\n"
    "      exit 1: sold-out\n"
    "    }\n"
    "  }\n"
    "\n"
    "  sequence shell::buy {\n"
    "    return 0\n"
    "  }\n"
    "\n"
    "  sequence shell::buy / sold-out {\n"
    "    return 1\n"
    "  }\n"
    "}\n";

const std::map<Kind, std::pair<std::string, std::string>>& writtenForms() {
  static const std::map<Kind, std::pair<std::string, std::string>> all = {
      {Kind::Package,
       {"package <name> { ... }\npackage <outer>@<inner> { ... }      reopens the nested package <inner>\nlanguage: cpp | c | "
        "python | typescript | lua     what its code is written in; what it holds says the same unless it says otherwise",
        "package shop {\n  module store { }\n}\n"}},
      {Kind::Module,
       {"[-] module <name> { ... }\nlanguage: <language>     only when it differs from the box around it",
        "package shop {\n  language: cpp\n  module store {\n    module pricing { }\n  }\n  module web {\n    language: python\n  }\n}\n"}},
      {Kind::Requirement,
       {"requirement <ID> {\n  title: <one sentence: The <subject> shall <verb> <object>>\n  status: draft | approved | obsolete\n  text:\n"
        "    <what it says, indented>\n}\nrequirement <ID>: <title>             the one-line form\nnested in a requirement, it refines "
        "it; elsewhere write @refines <ID>",
        "package shop {\n  requirement SREQ-00001-1 {\n    title: The shop shall take an order\n    status: draft\n"
        "    requirement REQ-00001-1: The cart shall total its items\n  }\n}\n"}},
      {Kind::Verification,
       {"verification <ID> {\n  title: <how it is proved>\n  method: test | review | analysis | inspection\n  tests:\n"
        "    <file>::<Fixture>.<Test>\n}\nnested in what it verifies, or with @requirement <ID>",
        "package shop {\n  requirement REQ-00001-1 {\n    title: The cart shall total its items\n    status: draft\n"
        "    verification VER-00001-1 {\n      title: Unit tests confirm the cart totals its items\n      method: test\n"
        "      tests:\n        tests/test_cart.cpp::TestCart.Totals\n    }\n  }\n}\n"}},
      {Kind::Constraint,
       {"constraint <ID>: <title>\nconstraint <ID> {\n  title: <what every instance of what it binds shall obey>\n  category: <category>\n"
        "  bok: <guides>\n}",
        "package shop {\n  constraint CST-00001-1: Every price shall be kept in cents\n}\n"}},
      {Kind::Interface,
       {"interface <name> {          in a package: an external interface of what the package builds\n  kind: cli | gui | http | ...\n"
        "  operation ...\n}",
        kShopInterface}},
      {Kind::Operation,
       {"operation <name>(<param>: <type>, ...) [-> <type>] [fails <Failure>, ... | fails nothing] {\n  command: <how it is invoked>\n  "
        "exit <code>: <scenario>     default, "
        "or a scenario of its sequence\n}\nsequence <interface>::<operation> [/ <scenario>] { ... } says how it is handled",
        kShopInterface}},
      {Kind::View,
       {"view <Name> {               in the state of an interface: its navigation\n  text: <what it shows>\n}",
        "package shop {\n  interface web {\n    kind: gui\n    operation buy(item: string) -> none\n  }\n\n  sequence web::buy {\n  }\n\n"
        "  state web {\n    initial Home\n    view Home {\n      text: The items, and a button to buy one.\n    }\n"
        "    view Done {\n      text: What was bought.\n    }\n    Home -> Done on buy\n    Done -> Home on back\n  }\n}\n"}},
      {Kind::Class,
       {"[+|-] class | dataclass <Name>[<T: Bound>] [: <Base>, ...] [<<operators>>] { members }\na base is written as the code "
        "writes it (IFormat<scene::SceneDocument>, Box[int]); its first name is what the class inherits, and a name the design does "
        "not declare is text (class Error : Exception)\nabstract is derived from abstract methods, never written",
        kShopClass}},
      {Kind::Enum,
       {"enum <Name> { <Value>, <Value> = 2, ... }",
        "package shop {\n  module store {\n    enum Status { Open, Paid, Shipped }\n  }\n}\n"}},
      {Kind::Function,
       {"[-] function <name>[<T>](<param>: <type>, ...) [-> <type>] [fails <Failure>, ... | fails nothing] [{ @requirement <ID> }]",
        "package shop {\n  module store {\n    function total(cents: const std::vector<int>&) -> int\n  }\n}\n"}},
      {Kind::Method,
       {"+|-|# [abstract | static | const] <name>[<T>](<param>: <type>, ...) [-> <type>] [fails <Failure>, ... | fails nothing] [{ "
        "body }]\nin a class; named like the class, "
        "it is a constructor; named ~ and the class, its destructor, which no interface counts against\na type is written as the code "
        "writes it; *args and **kwargs are parameter names like any other",
        kShopClass}},
      {Kind::Attribute,
       {"+|- [owns | refs | shares] [many | optional] <name>: <type> [= <default>]\nowns (the default) is composition, refs and "
        "shares aggregation; many is *, optional is 0..1, and without either the attribute holds one. The type is the code's text: "
        "each name in it that the design declares is related, any other is text",
        kShopClass}},
      {Kind::Operator,
       {"in the header:  class <Name> <<==, ordered, hash, print, +, -, *, []>>\nin the body:    operator <symbol>(<param>: <type>) [-> "
        "<type>]     refines one the header names",
        "package shop {\n  module money {\n    dataclass Cents <<==, ordered, +>> {\n      + value: int\n"
        "      operator +(other: Cents) -> Cents\n    }\n  }\n}\n"}},
      {Kind::Sequence,
       {"sequence <Class>::<method> [/ <scenario>] { ... }\nsequence <function> | <interface>::<operation> [/ <scenario>] { ... }\n"
        "a scenario is a path: written apart, or named on a branch\n"
        "fail <Failure> ends a path failing; attempt { ... } on <Failure> [/ <scenario>] { ... } handles what its steps fail "
        "with",
        kShopSequence}},
      {Kind::Message,
       {"[<from>] -> [<target>.]<op>(<args>) [/ <scenario>] [-> <result>]\nself -> <attribute>.<op>(<args>) -> <result>\nreturn "
        "<value>\nfail <Failure>\nlocal <name>: <type>",
        kShopSequence}},
      {Kind::Frame,
       {"loop <prose> { ... }\nopt <prose> [/ <scenario>] { ... }\nalt <prose> [/ <scenario>] { ... } else [/ <scenario>] { ... }\n"
        "attempt { ... } on <Failure> [/ <scenario>] { ... } [on ...]",
        kShopSequence}},
      {Kind::State,
       {"state <Class | interface> {\n  initial <State>\n  <A> -> <B> on <operation> [<guard, as prose>] / <action>\n}",
        "package shop {\n  module store {\n    class Order {\n      + pay() -> none\n      + ship() -> none\n    }\n\n"
        "    state Order {\n      initial Open\n      Open -> Paid on pay\n      Paid -> Shipped on ship\n    }\n  }\n}\n"}},
      {Kind::UseCase,
       {"usecase <ID> [<name>(<param>: <type>, ...) [-> <type>]] [: <generic ID>] {\n  title: <what it is for>\n  status: draft | "
        "approved | obsolete\n  @requirement <ID>\n  outcome default: <what is obtained>\n  outcome <scenario> [-> <type>]: <what is "
        "obtained instead>\n}\nan interface operation realises it: operation <name>(...) -> <type> : <UC ID>\nit names no "
        "interface: whoever drives the interface, the contract is the same",
        kShopUseCase}},
      {Kind::Rule,
       {"rule <name> {\n  title: <what it states>\n  @requirement <ID>\n  for <kind> <variable> [in <box>], ...\n  where <literals>\n"
        "  holds <literals>               or never <literals>\n}\nliterals joined by commas, each one of:\n  <a> <relation> <b>    a "
        "relation "
        "of the metamodel: c uses x, m satisfies r\n  <a> is <kind>         x is class\n  <a> in <b>            containment, at any "
        "depth\n "
        " "
        "<a> = <b>, !=, <, <=, >, >=   entities or values: numbers compare as numbers\n  <definition>(<a>, ...)\n  arg(<call>, <param>, "
        "<v>)"
        "   the call passes v, a literal written in its sequence, to param\n  not <literal>         once its variables are bound\na bare "
        "name "
        "is a variable; an entity is written qualified: shop@store, shop@store::Order; a value is a number or a \"string\"\nin a package, "
        "a module, a class or a member: a member's rule speaks about it and its calls, what a precondition says\nevaluated over the "
        "model: holds is broken by each binding with no answer, never by each binding with one",
        "package shop {\n  requirement REQ-00001-1: The store shall not depend on the web\n  module web {\n    class Page { }\n  }\n"
        "  module store {\n    class Order {\n      + total() -> int\n    }\n    rule store-blind-to-web {\n"
        "      title: No member of the store names a type of the web\n      @requirement REQ-00001-1\n      for class c in shop@store\n"
        "      never m in c, m uses x, x in shop@web\n    }\n  }\n}\n"}},
      {Kind::Definition,
       {"definition <name>(<parameter>, ...) {\n  title: <what it names>\n  if <literals>             one line per case in which it "
        "holds\n}\na rule or another definition applies it: <name>(<a>, ...); it may recur, and negate only what does not rest on it",
        "package shop {\n  module store {\n    class Order {\n      + total() -> int\n    }\n    definition member-of(m, c) {\n"
        "      title: A member and the class that holds it\n      if m in c, c is class\n    }\n  }\n}\n"}},
      {Kind::Outcome,
       {"outcome default: <what is obtained>           the ending that goes well\noutcome <scenario> [-> <type>]: <what is "
        "obtained>   any other: every operation that realises the use case handles it as a scenario",
        kShopUseCase}},
      {Kind::Diagram,
       {"diagram <Name> {            at the top of a file, outside any package\n  kind: class | sequence | state | requirement | "
        "interface | tour\n  include: <patterns>      shop@*, shop@store::*, shop@store::Cart\n  exclude, open, depth, show, hide: ...\n}",
        "package shop {\n  module store {\n    class Cart { }\n  }\n}\n\ndiagram CL_Store {\n  kind: class\n  include: "
        "shop@store::*\n}\n"}},
  };
  return all;
}

}  // namespace detail

std::string syntaxOf(Kind kind) {
  const auto it = detail::writtenForms().find(kind);
  return it == detail::writtenForms().end() ? "" : it->second.first;
}

std::string exampleOf(Kind kind) {
  const auto it = detail::writtenForms().find(kind);
  return it == detail::writtenForms().end() ? "" : it->second.second;
}

}  // namespace model
}  // namespace ds
