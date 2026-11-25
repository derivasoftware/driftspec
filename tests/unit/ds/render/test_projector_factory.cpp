#include <gtest/gtest.h>

#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/ClassProjector.hpp"
#include "ds/render/ProjectorFactory.hpp"
#include "ds/render/RequirementProjector.hpp"
#include "ds/render/SequenceProjector.hpp"
#include "ds/render/StateProjector.hpp"
#include "ds/render/TableProjector.hpp"
#include "ds/render/TourProjector.hpp"

namespace {

/// The one place that names the projectors (DS-369), by the kind of diagram they draw.
class TestProjectorFactory : public ::testing::Test {
 protected:
  template <typename Projector>
  bool makes(const std::string& kind) const {
    const auto projector = ds::render::ProjectorFactory::of(kind, model, metamodel);
    return projector && dynamic_cast<const Projector*>(projector.get()) != nullptr;
  }

  const ds::model::Metamodel metamodel;
  const ds::model::Model model{metamodel};
};

TEST_F(TestProjectorFactory, EachKindIsDrawnByItsOwnProjector) {
  EXPECT_TRUE(makes<ds::render::SequenceProjector>("sequence"));
  EXPECT_TRUE(makes<ds::render::TourProjector>("tour"));
  EXPECT_TRUE(makes<ds::render::RequirementProjector>("requirement"));
  EXPECT_TRUE(makes<ds::render::StateProjector>("state"));
  EXPECT_TRUE(makes<ds::render::TableProjector>("table"));
  EXPECT_TRUE(makes<ds::render::ClassProjector>("class"));
}

TEST_F(TestProjectorFactory, AKindWithNoProjectorOfItsOwnIsDrawnAsAClassDiagram) {
  EXPECT_TRUE(makes<ds::render::ClassProjector>(""));
  EXPECT_TRUE(makes<ds::render::ClassProjector>("component"));
}

}  // namespace
