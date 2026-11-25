#include "ds/emit/NotImplemented.hpp"

namespace ds {
namespace emit {

NotImplemented::NotImplemented(const std::string& document, const std::string& technology, const std::vector<std::string>& there)
    : std::logic_error(said(document, technology, there)), document_(document), technology_(technology) {}

const std::string& NotImplemented::document() const { return document_; }

const std::string& NotImplemented::technology() const { return technology_; }

std::string NotImplemented::said(const std::string& document, const std::string& technology, const std::vector<std::string>& there) {
  std::string out = (document.empty() ? "nothing is written in " : "the " + document + " is not written in ") + technology;
  if (there.empty()) return out;  // a sink that accepted nothing of a document knows no other that writes it (DS-399)
  out += "; there is:";
  for (const auto& t : there) out += " " + t;
  return out;
}

}  // namespace emit
}  // namespace ds
