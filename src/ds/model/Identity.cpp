#include "ds/model/Identity.hpp"

#include <regex>
#include <utility>

namespace ds {
namespace model {

namespace {
const std::string kSequence = "::sequence";
// artefact ids: PREFIX(-PART)*-NUMBER-REVISION, e.g. REQ-00001-2, DS-C-0001-0
const std::regex kArtefactWithRevision("^([A-Z]+(?:-[A-Z]+)*-[0-9]+)-([0-9]+)$");
const std::regex kArtefactBare("^[A-Z]+(?:-[A-Z]+)*-[0-9]+$");
const std::string kState = "::state";
const std::string kDiagram = "diagram:";

/// The scope outside `text`, of an identity or of a scope: both step out the same way.
std::string outerOf(const std::string& text) {
  const auto cut = text.rfind("::");
  if (cut != std::string::npos) return text.substr(0, cut);
  std::string s = text;
  if (!s.empty() && s.back() == '@') s.pop_back();  // a package scope: step out of that package
  const auto at = s.rfind('@');
  return at == std::string::npos ? "" : s.substr(0, at + 1);
}
}  // namespace

// ---- Identity ---------------------------------------------------------------------------------

Identity::Identity(std::string text) : text_(std::move(text)) {}

Identity Identity::ofDiagram(const std::string& name) { return Identity(name.rfind(kDiagram, 0) == 0 ? name : kDiagram + name); }

std::string Identity::lastName() const {
  const auto cut = text_.find_last_of(":@");
  return cut == std::string::npos ? text_ : text_.substr(cut + 1);
}

Scope Identity::scope() const { return Scope(outerOf(text_)); }

Scope Identity::asScope() const { return Scope(text_); }

bool Identity::isQualified() const { return text_.find('@') != std::string::npos || text_.find("::") != std::string::npos; }

bool Identity::isAbsolute() const { return text_.find('@') != std::string::npos; }

bool Identity::within(const Identity& outer) const {
  const std::string& prefix = outer.text_;
  return text_ == prefix || text_.rfind(prefix + "::", 0) == 0 || text_.rfind(prefix + "/", 0) == 0;
}

Identity Identity::withArity(std::size_t arity) const { return Identity(text_ + "<" + std::to_string(arity) + ">"); }

std::string Identity::diagramName() const { return text_.rfind(kDiagram, 0) == 0 ? text_.substr(kDiagram.size()) : text_; }

Identity Identity::sequence(const std::string& scenario) const {
  return Identity(text_ + kSequence + (scenario.empty() ? "" : "/" + scenario));
}

Identity Identity::state() const { return Identity(text_ + kState); }

Identity Identity::withScenario(const std::string& scenario) const { return Identity(text_ + "/" + scenario); }

Identity Identity::behaviour() const {
  const auto at = text_.find(kSequence);
  if (at == std::string::npos) return Identity("");
  const auto end = text_.find("::", at + kSequence.size());  // a message or a frame inside it
  return Identity(end == std::string::npos ? text_ : text_.substr(0, end));
}

std::string Identity::scenario() const {
  const std::string b = behaviour().text_;
  const auto slash = b.find('/', b.find(kSequence));
  return slash == std::string::npos ? "" : b.substr(slash + 1);
}

Identity Identity::subject() const {
  const auto at = text_.find(kSequence);
  return Identity(at == std::string::npos ? "" : text_.substr(0, at));
}

Identity Identity::machineSubject() const {
  const auto at = text_.rfind(kState);
  return Identity(at != std::string::npos && at + kState.size() == text_.size() ? text_.substr(0, at) : "");
}

// ---- Scope ------------------------------------------------------------------------------------

Scope::Scope(std::string text) : text_(std::move(text)) {}

Scope Scope::inside(const Identity& package) {
  const std::string& p = package.text();
  return Scope(!p.empty() && p.back() == '@' ? p : p + "@");
}

Identity Scope::qualify(const std::string& name) const {
  if (text_.empty()) return Identity(name);
  if (text_.back() == '@') return Identity(text_ + name);
  return Identity(text_ + "::" + name);
}

Scope Scope::outer() const { return Scope(outerOf(text_)); }

Scope Scope::package() const {
  const auto at = text_.rfind('@');
  return Scope(at == std::string::npos ? "" : text_.substr(0, at + 1));
}

Identity Scope::box() const { return Identity(!text_.empty() && text_.back() == '@' ? text_.substr(0, text_.size() - 1) : text_); }

bool Scope::isSystem() const { return !text_.empty() && text_.back() == '@' && text_.find('@') == text_.size() - 1; }

// ---- ArtefactName -----------------------------------------------------------------------------

ArtefactName::ArtefactName(std::string text) : text_(std::move(text)) {}

bool ArtefactName::matches(const std::string& text) {
  return std::regex_match(text, kArtefactBare) || std::regex_match(text, kArtefactWithRevision);
}

bool ArtefactName::hasRevision() const { return std::regex_match(text_, kArtefactWithRevision); }

ArtefactName ArtefactName::withoutRevision() const {
  std::smatch m;
  return ArtefactName(std::regex_match(text_, m, kArtefactWithRevision) ? m[1].str() : text_);
}

}  // namespace model
}  // namespace ds
