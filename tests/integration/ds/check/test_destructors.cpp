#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A class declares its destructor as ~Name() (REQ-00101-1, DS-143): a method flagged destructor, which
/// an interface may have, since GUIDE-cpp-design asks every interface for a virtual one.
class TestDestructors : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  module m {\n"
                                       "    class IStep {\n"
                                       "      + ~IStep()\n"
                                       "      + abstract name() -> string\n"
                                       "    }\n"
                                       "    class Pump {\n"
                                       "      + ~Pump()\n"
                                       "      + tick() -> bool\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  std::string stereotypes(const std::string& id) const {
    const auto* e = model.find(id);
    if (!e || !e->fields.count("stereotypes")) return "";
    return e->fields.at("stereotypes");
  }
};

TEST_F(TestDestructors, ATildeAndTheClassNameIsItsDestructor) {
  const auto* d = model.find("p@m::IStep::~IStep");
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->kind, ds::model::Kind::Method);
  EXPECT_TRUE(d->has("destructor"));
  EXPECT_TRUE(model.findings().empty());
}

TEST_F(TestDestructors, AnInterfaceKeepsItsDestructorAndStaysAnInterface) {
  EXPECT_EQ(stereotypes("p@m::IStep"), "abstract");
  EXPECT_EQ(stereotypes("p@m::Pump"), "");  // a concrete class with a destructor is not abstract
}

}  // namespace
