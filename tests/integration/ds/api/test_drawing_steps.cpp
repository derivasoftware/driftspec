#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Selection.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// A drawing loads only the steps of the check it reads (DS-376): every diagram of every kind is drawn, in every
/// format, byte for byte as it is drawn after the whole check; a step it does without changes nothing it shows.
class TestDrawingSteps : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const std::vector<std::string> formats{"svg", "text", "cells", "ir", "layout"};

  /// The .ds files under a directory of the repository, by path, as an editor holds them.
  static ds::api::Files filesOf(const std::string& dir) {
    ds::api::Files files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::string(DRIFTSPEC_SOURCE_DIR) + "/" + dir)) {
      if (entry.path().extension() != ".ds") continue;
      std::ifstream in(entry.path());
      std::stringstream text;
      text << in.rdbuf();
      files[entry.path().generic_string()] = text.str();
    }
    return files;
  }

  /// Each diagram drawn in each format from a workspace loaded for the whole model and from one loaded to draw it.
  void expectDrawnAlike(const ds::api::Files& files, const std::vector<std::string>& diagrams) const {
    const ds::api::Workspace whole(metamodel, files, ds::api::Output::Model);
    for (const auto& diagram : diagrams) {
      const ds::api::Workspace drawing(metamodel, files, ds::api::Output::Drawing, diagram);
      for (const auto& format : formats) {
        const std::string expected = whole.render(diagram, format, 0);
        EXPECT_FALSE(expected.empty()) << diagram << " " << format;
        EXPECT_EQ(drawing.render(diagram, format, 0), expected) << diagram << " " << format;
      }
      EXPECT_EQ(drawing.render(diagram, "svg", 480), whole.render(diagram, "svg", 480)) << diagram << " wrapped";
    }
  }

  /// A diagram's IR from the files bound and checked by the steps given, as a test chooses them.
  std::string irWith(const ds::api::Files& files, const std::string& diagram, const ds::check::Selection& selection) const {
    ds::model::Model model{metamodel};
    ds::bind::Binder binder(model, metamodel);
    ds::parse::Parser parser;
    for (const auto& f : files) binder.bind(parser.parseText(f.first, f.second));
    ds::check::CheckerFactory::make(model, metamodel, selection)->run(binder.notes());
    return ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false);
  }

  /// DriftSpec's design with a table of its rules, written as rows and columns: whether each holds.
  static ds::api::Files designWithTable() {
    ds::api::Files files = filesOf("design");
    files["tables.ds"] =
        "diagram T_DrawnRules {\n"
        "  kind: table\n"
        "  for rule r in driftspec@ds\n"
        "  column rule: r.title\n"
        "  column model: r.holds\n"
        "}\n";
    return files;
  }
};

TEST_F(TestDrawingSteps, EveryKindOfDiagramOfTheDesignIsDrawnAsAfterTheWholeCheck) {
  // classes (with relations and derived stereotypes), the HLD (with what behaviours call), an interface, a requirement
  // tree, a sequence, and a table written as rows and columns, whose cells are the rules' answers
  expectDrawnAlike(designWithTable(), {"CL_CheckNamespace", "HLD_Ds", "IF_Cli", "REQ_Language", "SEQ_Render", "T_DrawnRules"});
}

TEST_F(TestDrawingSteps, AStateMachineAndItsTableAreDrawnAsAfterTheWholeCheck) {
  expectDrawnAlike(filesOf("examples/states"), {"CL_Link", "ST_Link", "TB_Link", "SEQ_Lifetime"});
}

TEST_F(TestDrawingSteps, TheApiDrawsADiagramAsTheWorkspaceLoadedWholeDoes) {
  const ds::api::Files files = filesOf("examples/shop");
  const ds::api::Workspace whole(metamodel, files);
  for (const std::string diagram : {"CL_Shop", "HLD_ShopSales", "SEQ_Buy", "TOUR_Buy"})
    for (const std::string format : {"svg", "ir", "text"})
      EXPECT_EQ(ds::api::Api(metamodel).render(files, diagram, format, 0), whole.render(diagram, format, 0)) << diagram << " " << format;
}

/// The comparison sees a step a drawing needs: without the rules a table of them has no rows, without derivation
/// a class loses its stereotypes, without behaviour typing the HLD loses what its boxes call.
TEST_F(TestDrawingSteps, AStepADrawingReadsChangesItWhenItIsLeftOut) {
  using ds::check::Selection;
  using ds::check::Step;
  const ds::api::Files files = designWithTable();
  const Selection drawing = Selection::of({Step::Linker, Step::Behaviour, Step::Derivation});
  const Selection every = Selection::every();
  EXPECT_EQ(irWith(files, "T_DrawnRules", drawing.with(Step::Rules)), irWith(files, "T_DrawnRules", every));
  EXPECT_NE(irWith(files, "T_DrawnRules", drawing), irWith(files, "T_DrawnRules", every));
  EXPECT_NE(irWith(files, "CL_CheckNamespace", Selection::of({Step::Linker, Step::Behaviour})), irWith(files, "CL_CheckNamespace", every));
  EXPECT_NE(irWith(files, "HLD_Ds", Selection::of({Step::Linker, Step::Derivation})), irWith(files, "HLD_Ds", every));
}

}  // namespace
