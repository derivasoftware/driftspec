#include "ds/machine/Subject.hpp"

#include "ds/machine/WrittenFields.hpp"

namespace ds {
namespace machine {

const model::Entity* Subject::attribute(const std::string& name) const {
  const model::Entity* e = model_.find(id_ + "::" + name);
  return e && e->kind == model::Kind::Attribute ? e : nullptr;
}

std::vector<std::string> Subject::values(const std::string& name) const {
  const model::Entity* a = attribute(name);
  if (!a || a->field("default") == "true" || a->field("default") == "false") return {"false", "true"};
  for (const auto& r : model_.index().from(a->id)) {
    if (r.kind != "uses" && r.kind != "owns" && r.kind != "refs" && r.kind != "shares") continue;
    const model::Entity* type = model_.find(r.to);
    if (!type || type->kind != model::Kind::Enum) continue;
    std::vector<std::string> values;
    for (std::string v : WrittenFields::pieces(type->field("values"), ',')) {
      v.erase(0, v.find_first_not_of(' '));
      values.push_back(v);
    }
    return values;
  }
  return {};
}

}  // namespace machine
}  // namespace ds
