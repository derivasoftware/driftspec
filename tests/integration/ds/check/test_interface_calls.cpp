#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// A module offers interfaces like a package, and a behaviour calls an interface's operation through
/// a local, a parameter or an attribute typed by it (REQ-00093-1): the consumer depends on the
/// contract, not on the class behind it.
class TestInterfaceCalls : public ::testing::Test {
 protected:
  ds::model::Model model;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.relations())
      if (r.kind == kind && r.from == from && r.to == to) return true;
    return false;
  }

  void expectNoFinding() const {
    for (const auto& f : model.findings()) ADD_FAILURE() << f.code << " " << f.message;
  }

  static constexpr const char* kEngine =
      "package p {\n"
      "  interface engine {\n"
      "    kind: library\n"
      "    operation check(text: string) -> string\n"
      "  }\n"
      "  sequence engine::check {\n"
      "    local api: core::Api\n"
      "    -> api.check(text) -> json\n"
      "    return json\n"
      "  }\n"
      "  module core {\n"
      "    class Api {\n"
      "      @interface engine\n"
      "      + check(text: string) -> string\n"
      "    }\n"
      "  }\n";
};

TEST_F(TestInterfaceCalls, AModuleDeclaresAnInterfaceAndOffersIt) {
  load(std::string(kEngine) +
       "  module face {\n"
       "    interface port {\n"
       "      kind: library\n"
       "      operation read(n: int) -> string\n"
       "    }\n"
       "    sequence port::read {\n"
       "      return none\n"
       "    }\n"
       "  }\n"
       "}\n");
  const ds::model::Entity* port = model.find("p@face::port");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->kind, ds::model::Kind::Interface);
  EXPECT_NE(model.find("p@face::port::read"), nullptr);
  EXPECT_TRUE(related("exposes", "p@face", "p@face::port"));
  expectNoFinding();
}

TEST_F(TestInterfaceCalls, ABehaviourCallsAnInterfaceOperationThroughALocal) {
  load(std::string(kEngine) +
       "  module face {\n"
       "    function check(text: string) -> string\n"
       "    sequence check {\n"
       "      local ds: p@engine\n"
       "      -> ds.check(text) -> json\n"
       "      return json\n"
       "    }\n"
       "  }\n"
       "}\n");
  EXPECT_TRUE(related("calls", "p@face::check::sequence::m0", "p@engine::check"));
  EXPECT_TRUE(related("uses", "p@face::check::sequence", "p@engine"));
  expectNoFinding();
}

TEST_F(TestInterfaceCalls, ThroughAParameterOrAnAttributeTypedByTheInterface) {
  load(std::string(kEngine) +
       "  module face {\n"
       "    function check(ds: p@engine, text: string) -> string\n"
       "    sequence check {\n"
       "      -> ds.check(text) -> json\n"
       "      return json\n"
       "    }\n"
       "    class Client {\n"
       "      - refs engine: p@engine\n"
       "      + run(text: string) -> string\n"
       "    }\n"
       "    sequence Client::run {\n"
       "      -> engine.check(text) -> json\n"
       "      return json\n"
       "    }\n"
       "  }\n"
       "}\n");
  EXPECT_TRUE(related("calls", "p@face::check::sequence::m0", "p@engine::check"));
  EXPECT_TRUE(related("calls", "p@face::Client::run::sequence::m0", "p@engine::check"));
  EXPECT_TRUE(related("refs", "p@face::Client::engine", "p@engine"));
  expectNoFinding();
}

TEST_F(TestInterfaceCalls, TheSceneDrawsAModulesInterfaceInsideIt) {
  load(std::string(kEngine) +
       "  module face {\n"
       "    interface port {\n"
       "      kind: library\n"
       "      operation read(n: int) -> string\n"
       "    }\n"
       "    sequence port::read {\n"
       "      return none\n"
       "    }\n"
       "  }\n"
       "}\n");
  const auto document = ds::scene::Scene(model).build();
  bool inside = false;
  for (const auto& n : document.nodes) inside |= n.id == "p@face::port" && n.parent == "p@face";
  EXPECT_TRUE(inside);
}

}  // namespace
