#include "ds/model/Identity.hpp"

#include <utility>

namespace ds {
namespace model {

// ---- Identity ---------------------------------------------------------------------------------

const std::string Identity::kSequence = "::sequence";
const std::string Identity::kState = "::state";
const std::string Identity::kDiagram = "diagram:";

Identity::Identity(std::string text) : text_(std::move(text)) {}

Identity Identity::ofDiagram(const std::string& name) { return Identity(name.rfind(kDiagram, 0) == 0 ? name : kDiagram + name); }

std::string Identity::lastName() const {
  const auto cut = text_.find_last_of(":@");
  return cut == std::string::npos ? text_ : text_.substr(cut + 1);
}

Scope Identity::scope() const { return Scope(text_).outer(); }

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

Identity Identity::branch(bool elseSide) const { return Identity(text_ + (elseSide ? "::b1" : "::b0")); }

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

std::string Scope::outerOf(const std::string& text) {
  const auto cut = text.rfind("::");
  if (cut != std::string::npos) return text.substr(0, cut);
  std::string s = text;
  if (!s.empty() && s.back() == '@') s.pop_back();  // a package scope: step out of that package
  const auto at = s.rfind('@');
  return at == std::string::npos ? "" : s.substr(0, at + 1);
}

Scope Scope::outer() const { return Scope(outerOf(text_)); }

Scope Scope::package() const {
  const auto at = text_.rfind('@');
  return Scope(at == std::string::npos ? "" : text_.substr(0, at + 1));
}

Identity Scope::box() const { return Identity(!text_.empty() && text_.back() == '@' ? text_.substr(0, text_.size() - 1) : text_); }

bool Scope::isSystem() const { return !text_.empty() && text_.back() == '@' && text_.find('@') == text_.size() - 1; }

// ---- ArtefactName -----------------------------------------------------------------------------

int ArtefactName::numberParts(const std::string& text, std::size_t* lastDash) {
  // one pass over the parts between dashes, no copy: letter parts first, then number parts (DS-335)
  int letters = 0;
  int numbers = 0;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i < text.size() && text[i] != '-') continue;
    if (i == start) return -1;  // an empty part: a dash that leads, trails or doubles
    bool upper = true;
    bool digit = true;
    for (std::size_t k = start; k < i; ++k) {
      upper = upper && text[k] >= 'A' && text[k] <= 'Z';
      digit = digit && text[k] >= '0' && text[k] <= '9';
    }
    if (upper && numbers == 0) {
      ++letters;
    } else if (digit && letters > 0) {
      ++numbers;
    } else {
      return -1;
    }
    if (i < text.size() && lastDash) *lastDash = i;
    start = i + 1;
  }
  return numbers;
}

bool ArtefactName::writtenWithRevision(const std::string& text, std::string* stem) {
  std::size_t lastDash = 0;
  if (numberParts(text, &lastDash) != 2) return false;
  if (stem) *stem = text.substr(0, lastDash);
  return true;
}

bool ArtefactName::writtenBare(const std::string& text) { return numberParts(text, nullptr) == 1; }

ArtefactName::ArtefactName(std::string text) : text_(std::move(text)) {}

bool ArtefactName::matches(const std::string& text) { return writtenBare(text) || writtenWithRevision(text); }

bool ArtefactName::hasRevision() const { return writtenWithRevision(text_); }

ArtefactName ArtefactName::withoutRevision() const {
  std::string stem;
  return ArtefactName(writtenWithRevision(text_, &stem) ? stem : text_);
}

}  // namespace model
}  // namespace ds
