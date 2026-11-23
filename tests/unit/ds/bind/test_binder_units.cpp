#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The binder alone (DS-186): a file read into the model's entities, with the references it leaves
/// for the linker and the messages and events it leaves for the behaviour steps.
TEST(TestBinder, AFileBecomesEntitiesAndTheReferencesItCannotResolveAlone) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  module m {\n"
                               "    class Stock { + take() -> none }\n"
                               "    class Till : Base {\n"
                               "      - stock: Stock\n"
                               "      + sell() -> none\n"
                               "    }\n"
                               "    sequence Till::sell {\n"
                               "      self -> stock.take()\n"
                               "    }\n"
                               "    state Stock {\n"
                               "      initial Full\n"
                               "      Full -> Empty on take\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  ASSERT_NE(model.find("p@m::Till"), nullptr);
  ASSERT_NE(model.find("p@m::Till::sell"), nullptr);
  ASSERT_NE(model.find("p@m::Till::sell::sequence"), nullptr);
  bool base = false;
  for (const auto& r : binder.notes().references) base |= r.from == "p@m::Till" && r.target == "Base" && r.relation == "inherits";
  EXPECT_TRUE(base);  // the binder names it; the linker resolves it
  EXPECT_FALSE(binder.notes().messages.empty());
  EXPECT_FALSE(binder.notes().events.empty());
}

}  // namespace
