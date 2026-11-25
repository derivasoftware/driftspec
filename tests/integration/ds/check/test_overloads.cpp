#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// Overloads (DS-455), over tests/corpus/overloads: two operations of one name in one owner, each with its parameter
/// types in its identity; calls narrowed by their arguments; a call or a reference that two still match names none
/// (its `!` lines are the oracle).
class TestOverloads : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/overloads")) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }
  /// What the messages of a behaviour call, in the order written.
  std::vector<std::string> called(const std::string& sequence) const {
    std::vector<std::string> out;
    for (const auto& m : model.index().members(sequence))
      for (const auto& r : model.index().fromOfKind(m, "calls")) out.push_back(r.to);
    std::sort(out.begin(), out.end());
    return out;
  }
  std::vector<std::string> said(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

/// An owner may declare two operations of one name: each is its name with its parameter types as written, a class's
/// methods, an interface's operations and a module's functions alike; one not overloaded keeps its bare name.
TEST_F(TestOverloads, EachOverloadIsItsNameWithItsParameterTypesAndOneAloneKeepsItsBareName) {
  for (const char* id : {"shop@catalogue::Factory::of(Scene)", "shop@catalogue::Factory::of(Report, int)", "shop@catalogue::Factory::of()",
                         "shop@catalogue::IMaker::make(int)", "shop@catalogue::IMaker::make(Scene)", "shop@catalogue::shown(int)",
                         "shop@catalogue::shown(Scene)", "shop@catalogue::Factory::single"}) {
    const ds::model::Entity* e = model.find(id);
    ASSERT_NE(e, nullptr) << id;
    EXPECT_EQ(e->name, ds::model::Identity(id).bare().lastName()) << id;  // its name stays bare
  }
  EXPECT_EQ(model.find("shop@catalogue::Factory::of"), nullptr);
  for (const auto& f : model.findings()) EXPECT_NE(f.code, "E-DUPLICATE-IDENTITY") << f.message;
}

/// A call is narrowed by its arguments first: one left is the one it calls; a call two still match calls none, and is
/// W-NAME-AMBIGUOUS naming both. A behaviour names its overload by its types; one named bare when three share the name
/// is W-NAME-AMBIGUOUS too.
TEST_F(TestOverloads, ACallIsNarrowedByItsArgumentsAndOneThatTwoStillMatchIsAmbiguous) {
  EXPECT_EQ(called("shop@catalogue::Desk::open::sequence"),
            (std::vector<std::string>{"shop@catalogue::Factory::of()", "shop@catalogue::Factory::of(Scene)"}));
  EXPECT_EQ(called("shop@counter::Caller::run::sequence"), std::vector<std::string>{});
  const auto behaviourOf = model.index().fromOfKind("shop@catalogue::Factory::of(Scene)::sequence", "behaviour-of");
  ASSERT_EQ(behaviourOf.size(), 1u);
  EXPECT_EQ(behaviourOf.front().to, "shop@catalogue::Factory::of(Scene)");
  const auto ambiguous = said("W-NAME-AMBIGUOUS");
  ASSERT_EQ(ambiguous.size(), 2u);
  EXPECT_TRUE(std::any_of(ambiguous.begin(), ambiguous.end(),
                          [](const std::string& m) { return m.find("3 operations of that name") != std::string::npos; }));
  EXPECT_TRUE(std::any_of(ambiguous.begin(), ambiguous.end(),
                          [](const std::string& m) { return m.find("calls go with 1 argument") != std::string::npos; }));
}

}  // namespace
