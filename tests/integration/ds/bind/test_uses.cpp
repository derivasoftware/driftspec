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

/// A box declares that it uses an interface another box offers (REQ-00088-1): @uses binds a
/// consumes relation, the same in the box or in an extension block in another file.
class TestUses : public ::testing::Test {
 protected:
  ds::model::Model model;

  void load(const std::vector<std::pair<std::string, std::string>>& files) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : files) {
      const auto parsed = parser.parseText(f.first, f.second);
      ASSERT_FALSE(parsed.hasSyntaxErrors) << f.first << ": " << parsed.errorDetail;
      binder.bind(parsed);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  bool consumes(const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all())
      if (r.kind == "consumes" && r.from == from && r.to == to) return true;
    return false;
  }

  std::vector<std::string> codes() const {
    std::vector<std::string> out;
    for (const auto& f : model.findings()) out.push_back(f.code);
    return out;
  }

  static constexpr const char* kSystem =
      "package sistema {\n"
      "  package broker {\n"
      "    interface publish {\n"
      "      kind: api\n"
      "    }\n"
      "    interface observe {\n"
      "      kind: api\n"
      "    }\n"
      "  }\n"
      "  package protocol {\n"
      "    module messages {\n"
      "      dataclass Dato { }\n"
      "    }\n"
      "  }\n";
};

TEST_F(TestUses, APackageAModuleOrAClassUsesAnInterfaceOfAnotherBox) {
  load({{"a.ds", std::string(kSystem) + "  package service {\n"
                                        "    @uses sistema@broker@publish\n"
                                        "  }\n"
                                        "  package tui {\n"
                                        "    module screen {\n"
                                        "      @uses sistema@broker@observe\n"
                                        "      class View {\n"
                                        "        @uses sistema@broker@observe\n"
                                        "        + show(n: int) -> none\n"
                                        "      }\n"
                                        "    }\n"
                                        "  }\n"
                                        "}\n"}});
  EXPECT_TRUE(consumes("sistema@service", "sistema@broker@publish"));
  EXPECT_TRUE(consumes("sistema@tui@screen", "sistema@broker@observe"));
  EXPECT_TRUE(consumes("sistema@tui@screen::View", "sistema@broker@observe"));
  for (const auto& c : codes()) EXPECT_TRUE(c.rfind("W-OPERATION", 0) == 0) << c;  // nothing about the uses
}

TEST_F(TestUses, AUseWrittenInAnotherFileBindsTheSame) {
  load({{"a.ds", std::string(kSystem) + "  package service { }\n  package web { }\n}\n"},
        {"wiring.ds",  // the system's wiring, apart from the boxes it connects
         "package sistema {\n"
         "  package service {\n"
         "    @uses sistema@broker@publish\n"
         "  }\n"
         "  package web {\n"
         "    @uses sistema@broker@observe\n"
         "  }\n"
         "}\n"}});
  EXPECT_TRUE(consumes("sistema@service", "sistema@broker@publish"));
  EXPECT_TRUE(consumes("sistema@web", "sistema@broker@observe"));
}

TEST_F(TestUses, AUseOfSomethingThatIsNoInterfaceIsReportedAndNotBound) {
  load({{"a.ds", std::string(kSystem) + "  package service {\n"
                                        "    @uses sistema@protocol@messages::Dato\n"
                                        "  }\n"
                                        "}\n"}});
  int kind = 0;
  for (const auto& c : codes()) kind += c == "E-RELATION-KIND" ? 1 : 0;
  EXPECT_EQ(kind, 1);
  for (const auto& r : model.index().all()) EXPECT_FALSE(r.kind == "consumes" && r.from == "sistema@service") << r.to;
}

TEST_F(TestUses, AnAnnotationMeansWhatTheMetamodelSaysWhereverItIsWritten) {
  const auto uses = ds::model::annotationOf("uses");
  EXPECT_EQ(uses.relation, "consumes");
  EXPECT_EQ(uses.target, ds::model::Kind::Interface);
  EXPECT_EQ(ds::model::annotationOf("whatever").relation, "refers");  // a word with no meaning stays a plain link
  EXPECT_EQ(ds::model::annotationOf("requirement").onVerification, "verifies");
  for (const auto& a : ds::model::annotations()) {
    if (a.relation == "refers" || a.name.rfind("@", 0) != 0) continue;
    EXPECT_NE(ds::model::relationOf(a.relation), nullptr) << a.name;
    if (!a.onVerification.empty()) {
      EXPECT_NE(ds::model::relationOf(a.onVerification), nullptr) << a.name;
    }
  }
}

}  // namespace
