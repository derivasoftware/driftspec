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

/// The principles of the service and its doors in C++, as rules of ds's design (DS-406, DS-476): a use case knows no
/// door, only UseCaseFactory names a use case, and neither the api nor the service holds a function. The design of ds
/// keeps every one, and the same design with one deliberate violation written into it is told which rule it breaks.
class TestServicePrinciples : public ::testing::Test {
 protected:
  /// A line to write into one file of ds's design, after the line that starts with `after`.
  struct Edit {
    std::string file;   ///< the file's path under design/, as ds/service.ds
    std::string after;  ///< the start of the line it goes after; it must occur once
    std::string line;   ///< what is written, ending in a newline
  };

  /// The rules of ds's design that a model breaks: the rule's identity, once per finding.
  static std::vector<std::string> broken(const std::vector<Edit>& edits) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (auto file : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) {
      for (const Edit& edit : edits) {
        const std::string suffix = "/design/" + edit.file;
        if (file.path.size() < suffix.size() || file.path.compare(file.path.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        const std::size_t at = file.text.find("\n" + edit.after);
        EXPECT_NE(at, std::string::npos) << edit.after;
        EXPECT_EQ(file.text.find("\n" + edit.after, at + 1), std::string::npos) << edit.after << " occurs twice";
        const std::size_t end = file.text.find('\n', at + 1) + 1;
        file = parser.parseText(file.path, file.text.substr(0, end) + edit.line + file.text.substr(end));
      }
      binder.bind(file);
    }
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == "E-RULE-BROKEN") out.push_back(f.message.substr(0, f.message.find(' ')));
    return out;
  }
  static bool has(const std::vector<std::string>& rules, const std::string& rule) {
    for (const auto& r : rules)
      if (r == rule) return true;
    return false;
  }

  const std::string noDoor = "driftspec@ds@service::use-cases-know-no-door";
  const std::string noSibling = "driftspec@ds@service::no-use-case-knows-another";
  const std::string throughInterface = "driftspec@ds@families-are-reached-through-their-interface";
  const std::string apiClasses = "driftspec@ds@api::api-operations-belong-to-classes";
  const std::string serviceClasses = "driftspec@ds@service::service-operations-belong-to-classes";
};

TEST_F(TestServicePrinciples, TheDesignOfDsKeepsTheRulesOfTheService) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {noDoor, noSibling, throughInterface, apiClasses, serviceClasses}) EXPECT_FALSE(has(rules, rule)) << rule;
}

/// A use case knows no door (REQ-00207-1): one that holds the workspace, one whose behaviour runs the command line's
/// main, and one that names the Python face's sink are each told.
TEST_F(TestServicePrinciples, AUseCaseThatKnowsADoorIsTold) {
  const std::vector<std::string> api =
      broken({{"ds/service.ds", "    class CheckDesign : IUseCase {", "      - owns workspace_: api::Workspace\n"}});
  EXPECT_TRUE(has(api, noDoor));
  EXPECT_FALSE(has(api, throughInterface));  // the workspace is in no family: only the service's rule sees it
  const std::vector<std::string> app =
      broken({{"ds/service.ds", "    class HandModel : IUseCase {", "      + exitOf(argv: std::vector<std::string>) -> int\n"},
              {"ds/service.ds", "  module service {",
               "    sequence HandModel::exitOf {\n"
               "      -> app::main(argv) -> code\n"
               "      return code\n"
               "    }\n"}});
  EXPECT_TRUE(has(app, noDoor));  // a call is knowing too, with no local to depend on
  const std::vector<std::string> python =
      broken({{"ds/service.ds", "    class HandModel : IUseCase {", "      + into(sink: driftspec@driftspec@_native::PySink) -> void\n"}});
  EXPECT_TRUE(has(python, noDoor));
  EXPECT_FALSE(has(python, noSibling));
}

/// Only UseCaseFactory names a use case (REQ-00208-1): a door's class that names one is told by the convention of the
/// families (DS-405), whether it holds it, makes it in a behaviour or is handed it.
TEST_F(TestServicePrinciples, ADoorThatNamesAUseCaseIsTold) {
  const std::vector<std::string> held = broken({{"ds/api.ds", "    class Workspace {", "      - owns checks_: service::CheckDesign\n"}});
  EXPECT_TRUE(has(held, throughInterface));
  EXPECT_FALSE(has(held, noSibling));  // a door is no use case
  const std::vector<std::string> made =
      broken({{"ds/api.ds", "    sequence Workspace::run {", "      local checks: service::CheckDesign\n"}});
  EXPECT_TRUE(has(made, throughInterface));
  const std::vector<std::string> handed = broken({{"ds/api.ds", "    class Browser {", "      - see(sees: service::SeeDesign) -> void\n"}});
  EXPECT_TRUE(has(handed, throughInterface));
}

/// A use case that names another is in the family, which the convention of the families leaves free: the service's
/// own rule tells it (REQ-00208-1), as bind's, check's and render's do for their siblings.
TEST_F(TestServicePrinciples, AUseCaseThatKnowsAnotherIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/service.ds", "    class SeeDesign : IUseCase {", "      - owns checks_: CheckDesign\n"}});
  EXPECT_TRUE(has(rules, noSibling));
  EXPECT_FALSE(has(rules, throughInterface));  // a sibling of the family: only the service's rule sees it
  EXPECT_FALSE(has(rules, noDoor));
}

/// A function of the api is told, a public one or one hidden in the module (REQ-00209-1): how a cut file is read, what
/// the engine answers and how an identity is written are each a class's.
TEST_F(TestServicePrinciples, AFunctionOfTheApiIsTold) {
  const std::vector<std::string> loose =
      broken({{"ds/api.ds", "  module api {", "    function unclosedBlock(text: std::string) -> int\n"}});
  EXPECT_TRUE(has(loose, apiClasses));
  EXPECT_FALSE(has(loose, serviceClasses));
  const std::vector<std::string> hidden =
      broken({{"ds/api.ds", "  module api {", "    - function unclosedBlock(text: std::string) -> int\n"}});
  EXPECT_TRUE(has(hidden, apiClasses));
}

/// A function of the service is told, a public one or one hidden in the module (REQ-00209-1): what a use case reads of
/// its request is the Request's.
TEST_F(TestServicePrinciples, AFunctionOfTheServiceIsTold) {
  const std::vector<std::string> loose =
      broken({{"ds/service.ds", "  module service {", "    function number(request: Request, name: std::string) -> int\n"}});
  EXPECT_TRUE(has(loose, serviceClasses));
  EXPECT_FALSE(has(loose, apiClasses));
  const std::vector<std::string> hidden =
      broken({{"ds/service.ds", "  module service {", "    - function number(request: Request, name: std::string) -> int\n"}});
  EXPECT_TRUE(has(hidden, serviceClasses));
  EXPECT_FALSE(has(hidden, noDoor));
}

}  // namespace
