#include "ds/model/Identity.hpp"

namespace ds {
namespace model {

namespace {
const std::string kSequence = "::sequence";
const std::string kState = "::state";
const std::string kDiagram = "diagram:";
}  // namespace

std::string qualify(const std::string& scope, const std::string& name) {
  if (scope.empty()) return name;
  if (scope.back() == '@') return scope + name;
  return scope + "::" + name;
}

std::string outer(const std::string& scope) {
  const auto cut = scope.rfind("::");
  if (cut != std::string::npos) return scope.substr(0, cut);
  std::string s = scope;
  if (!s.empty() && s.back() == '@') s.pop_back();  // a package scope: step out of that package
  const auto at = s.rfind('@');
  return at == std::string::npos ? "" : s.substr(0, at + 1);
}

bool isQualified(const std::string& name) { return name.find('@') != std::string::npos || name.find("::") != std::string::npos; }

bool isAbsolute(const std::string& name) { return name.find('@') != std::string::npos; }

std::string boxOf(const std::string& scope) { return !scope.empty() && scope.back() == '@' ? scope.substr(0, scope.size() - 1) : scope; }

std::string packageOf(const std::string& scope) {
  const auto at = scope.rfind('@');
  return at == std::string::npos ? "" : scope.substr(0, at + 1);
}

std::string scopeIn(const std::string& package) { return !package.empty() && package.back() == '@' ? package : package + "@"; }

bool isSystemScope(const std::string& scope) { return !scope.empty() && scope.back() == '@' && scope.find('@') == scope.size() - 1; }

std::string lastName(const std::string& id) {
  const auto cut = id.find_last_of(":@");
  return cut == std::string::npos ? id : id.substr(cut + 1);
}

bool within(const std::string& id, const std::string& prefix) {
  return id == prefix || id.rfind(prefix + "::", 0) == 0 || id.rfind(prefix + "/", 0) == 0;
}

std::string behaviourId(const std::string& subject, const std::string& scenario) {
  return subject + kSequence + (scenario.empty() ? "" : "/" + scenario);
}

std::string stateId(const std::string& subject) { return subject + kState; }

std::string withScenario(const std::string& behaviour, const std::string& scenario) { return behaviour + "/" + scenario; }

std::string subjectOf(const std::string& id) {
  const auto at = id.find(kSequence);
  return at == std::string::npos ? "" : id.substr(0, at);
}

std::string machineSubjectOf(const std::string& stateMachine) {
  const auto at = stateMachine.rfind(kState);
  return at != std::string::npos && at + kState.size() == stateMachine.size() ? stateMachine.substr(0, at) : "";
}

std::string behaviourOf(const std::string& id) {
  const auto at = id.find(kSequence);
  if (at == std::string::npos) return "";
  const auto end = id.find("::", at + kSequence.size());  // a message or a frame inside it
  return end == std::string::npos ? id : id.substr(0, end);
}

std::string scenarioOf(const std::string& id) {
  const std::string behaviour = behaviourOf(id);
  const auto slash = behaviour.find('/', behaviour.find(kSequence));
  return slash == std::string::npos ? "" : behaviour.substr(slash + 1);
}

std::string arityId(const std::string& name, std::size_t arity) { return name + "<" + std::to_string(arity) + ">"; }

std::string diagramId(const std::string& name) { return name.rfind(kDiagram, 0) == 0 ? name : kDiagram + name; }

std::string diagramName(const std::string& id) { return id.rfind(kDiagram, 0) == 0 ? id.substr(kDiagram.size()) : id; }

}  // namespace model
}  // namespace ds
