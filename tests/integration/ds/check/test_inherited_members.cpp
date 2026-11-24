#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A class's members include what it inherits (REQ-00036-2, DS-105): a behaviour calls an inherited
/// method and reaches an inherited attribute as its own, and a private one of the base stays hidden.
class TestInheritedMembers : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  module m {\n"
                                       "    class Store {\n"
                                       "      + get(key: string) -> string\n"
                                       "    }\n"
                                       "    class Route {\n"
                                       "      # store: Store\n"
                                       "      # encode(value: string) -> string\n"
                                       "      - secret() -> string\n"
                                       "      + abstract handle(key: string) -> string\n"
                                       "    }\n"
                                       "    class LatestRoute : Route {\n"
                                       "      + handle(key: string) -> string\n"
                                       "    }\n"
                                       "    class Server {\n"
                                       "      - owns route: LatestRoute\n"
                                       "      + serve(key: string) -> string\n"
                                       "    }\n"
                                       "    sequence LatestRoute::handle {\n"
                                       "      self -> store.get(key) -> value\n"
                                       "      self -> encode(value) -> json\n"
                                       "      self -> secret() -> s\n"
                                       "      return json\n"
                                       "    }\n"
                                       "    sequence Server::serve {\n"
                                       "      self -> route.encode(key) -> json\n"
                                       "      return json\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  bool calls(const std::string& to) const {
    for (const auto& r : model.index().all())
      if (r.kind == "calls" && r.to == to) return true;
    return false;
  }

  std::vector<std::string> codes(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }
};

TEST_F(TestInheritedMembers, ASelfCallReachesAMethodTheClassInherits) {
  EXPECT_TRUE(calls("p@m::Route::encode"));
  EXPECT_TRUE(codes("W-MSG-UNDECLARED").empty());
}

TEST_F(TestInheritedMembers, AnInheritedAttributeIsAParticipant) {
  EXPECT_TRUE(calls("p@m::Store::get"));
  EXPECT_TRUE(codes("W-MSG-UNDECLARED").empty());
}

TEST_F(TestInheritedMembers, ACallOnASubclassReachesTheMethodOfItsBase) {
  // reached through the derived type from outside the class: the inherited method is found where it is declared
  EXPECT_EQ(codes("W-MSG-UNDECLARED"), std::vector<std::string>{});
}

TEST_F(TestInheritedMembers, APrivateMemberOfTheBaseStaysHidden) {
  EXPECT_TRUE(calls("p@m::Route::secret"));
  const auto hidden = codes("W-NOT-VISIBLE");
  ASSERT_EQ(hidden.size(), 2u);
  EXPECT_NE(hidden[0].find("p@m::Route::secret, which is private"), std::string::npos);
}

TEST_F(TestInheritedMembers, AProtectedMemberIsSeenFromADerivedClassAndHiddenFromAnyOther) {
  // LatestRoute derives from Route and calls encode: seen. Server calls route.encode from outside: hidden (DS-224)
  const auto hidden = codes("W-NOT-VISIBLE");
  int fromServer = 0, fromDerived = 0;
  for (const auto& m : hidden) {
    fromServer += m.find("Server::serve") != std::string::npos && m.find("Route::encode, which is protected") != std::string::npos;
    fromDerived += m.find("LatestRoute::handle") != std::string::npos && m.find("encode") != std::string::npos;
  }
  EXPECT_EQ(fromServer, 1);
  EXPECT_EQ(fromDerived, 0);
}

}  // namespace
