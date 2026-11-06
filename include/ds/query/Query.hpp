#ifndef DRIFTSPEC_QUERY_QUERY_HPP
#define DRIFTSPEC_QUERY_QUERY_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace query {

/// One line of an answer: an entity named by its identity, with what the question asked about it.
struct Answer {
  std::string kind;    ///< the entity's kind, or the relation's kind for a relation question
  std::string id;      ///< the identity the answer names (follow it with another question or a drawing)
  std::string detail;  ///< what else the question asked: a direction, a diagram kind, a title, a finding
};

/// Questions about the model, answered by identity (SREQ-00011-1): what exists,
/// what an entity relates to, what a diagram shows, what proves a class.
class Query {
 public:
  explicit Query(const model::Model& model) : model_(model) {}

  /// The questions `ask` understands, with the argument each takes.
  static std::vector<std::pair<std::string, std::string>> questions();

  /// Answer `question` about `subject` ("" for the questions that take none); an unknown question,
  /// a missing identity or one the model does not declare yields one answer of kind "error".
  std::vector<Answer> ask(const std::string& question, const std::string& subject) const;

  std::vector<Answer> diagrams() const;                          ///< every diagram, with its kind
  std::vector<Answer> entities(const std::string& scope) const;  ///< the direct members of a scope ("" = the top level)
  std::vector<Answer> relations(const std::string& id) const;    ///< every relation touching `id`, with its direction
  std::vector<Answer> artefacts(const std::string& id) const;    ///< the requirements and constraints on `id`, and what verifies them
  std::vector<Answer> behaviours(const std::string& id) const;   ///< the sequences and state machines of a method or class
  std::vector<Answer> shows(const std::string& diagram) const;   ///< the entities a diagram draws
  std::vector<Answer> findings(const std::string& file) const;   ///< the findings of a file ("" = all)
  std::vector<Answer> coupling() const;                          ///< what each behaviour touches outside its module, and how

  /// The answers as lines (`kind  id  detail`) or as a JSON array.
  static std::string text(const std::vector<Answer>& answers);
  static std::string json(const std::vector<Answer>& answers);

 private:
  bool known(const std::string& id) const;                         // an entity, or a scope something is declared in
  std::vector<std::string> nearTo(const std::string& name) const;  // identities ending in the name, for a hint
  const model::Model& model_;
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_QUERY_HPP
