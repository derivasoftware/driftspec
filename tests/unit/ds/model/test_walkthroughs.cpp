#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Walkthroughs.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;

/// A walkthrough built by hand, as the binder and the linker leave it (DS-471): two steps declared out of order, the
/// second of which names a class that is not declared.
Model walk(const ds::model::Metamodel& metamodel) {
  Model model{metamodel};
  const auto declare = [&model](Kind kind, const std::string& id, const std::string& name, std::map<std::string, std::string> fields = {}) {
    Entity e;
    e.kind = kind;
    e.id = id;
    e.name = name;
    e.fields = std::move(fields);
    model.declare(e);
    return e;
  };
  const Entity w = declare(Kind::Diagram, "diagram:WALK_A", "WALK_A", {{"kind", "walkthrough"}, {"title", "A walk"}});
  declare(Kind::Diagram, "diagram:CL_A", "CL_A");
  declare(Kind::Class, "p@m::A", "A");
  for (const int n : {2, 1}) {
    Entity s;
    s.kind = Kind::Step;
    s.id = "diagram:WALK_A::" + std::to_string(n);
    s.name = "step " + std::to_string(n);
    s.scope = w.id;
    s.fields["show"] = "CL_A";
    s.fields["text"] = "says " + std::to_string(n);
    model.declare(s);
    model.relate({"contains", w.id, s.id, {}, ""});
    model.relate({"shows", s.id, "diagram:CL_A", {}, ""});
  }
  model.relate({"focuses", "diagram:WALK_A::1", "p@m::A", {}, ""});
  model.relate({"points-at", "diagram:WALK_A::1", "p@m::A", {}, ""});
  model.relate({"highlights", "diagram:WALK_A::2", "p@m::Gone", {}, ""});
  return model;
}

TEST(TestWalkthroughs, EachStepIsGivenInTheOrderWrittenWithWhatResolves) {
  const ds::model::Metamodel metamodel;
  const Model model = walk(metamodel);
  const auto all = ds::model::Walkthroughs::of(model);
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].id, "diagram:WALK_A");
  EXPECT_EQ(all[0].title, "A walk");
  ASSERT_EQ(all[0].steps.size(), 2u);
  EXPECT_EQ(all[0].steps[0].n, 1);
  EXPECT_EQ(all[0].steps[0].diagram, "diagram:CL_A");
  EXPECT_EQ(all[0].steps[0].focus, std::vector<std::string>{"p@m::A"});
  EXPECT_EQ(all[0].steps[0].at, "p@m::A");
  EXPECT_EQ(all[0].steps[0].text, "says 1");
  EXPECT_TRUE(all[0].steps[1].highlight.empty());  // a name that resolves to nothing is the check's to tell
}

TEST(TestWalkthroughs, AModelWithNoWalkthroughHasNone) {
  const ds::model::Metamodel metamodel;
  const Model model{metamodel};
  EXPECT_TRUE(ds::model::Walkthroughs::of(model).empty());
}

}  // namespace
