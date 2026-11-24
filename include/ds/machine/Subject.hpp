#ifndef DRIFTSPEC_MACHINE_SUBJECT_HPP
#define DRIFTSPEC_MACHINE_SUBJECT_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

/// The class a state machine is of (DS-249): the attributes its rules name, and the values each may take.
class Subject {
 public:
  Subject(const model::Model& model, const std::string& id) : model_(model), id_(id) {}
  /// The attribute of the subject a name is; null when it is none: then it is a condition with a name.
  const model::Entity* attribute(const std::string& name) const;
  /// The values a name may take: those of its enum; true and false for a condition with a name, or for an
  /// attribute whose initial value is true or false (ds reads no type's name: DS-18); empty when they cannot
  /// be listed (a number, a text, a time).
  std::vector<std::string> values(const std::string& name) const;

 private:
  const model::Model& model_;
  std::string id_;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_SUBJECT_HPP
