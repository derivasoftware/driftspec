#ifndef DRIFTSPEC_QUERY_QUERY_HPP
#define DRIFTSPEC_QUERY_QUERY_HPP

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ds/model/Manual.hpp"
#include "ds/model/Model.hpp"
#include "ds/query/Filter.hpp"

namespace ds {
namespace query {

/// One line of an answer: an entity named by its identity, with what the question asked about it.
struct Answer {
  std::string kind;       ///< the entity's kind, or the relation's kind for a relation question
  std::string id;         ///< the identity the answer names (follow it with another question or a drawing)
  std::string detail;     ///< what else the question asked: a direction, a diagram kind, a title, a finding
  std::string from = {};  ///< for a relation question, the relation's two ends as the model holds them (DS-445);
  std::string to = {};    ///< "" for every other question, whose answers carry neither
};

/// The query module's entry point (DS-127): what the rest of ds asks of the model. A named question
/// answered by identity, or the model taken through a filter the caller gives.
class IQuery {
 public:
  virtual ~IQuery() = default;
  virtual std::vector<Answer> ask(const std::string& question, const std::string& subject) const = 0;
  virtual std::set<std::string> select(const IFilter& filter) const = 0;
};

/// Questions about the model, answered by identity (SREQ-00011-1): what exists, what an entity
/// relates to, what a diagram shows, what proves a class; and any filter it is given, which it hands
/// the model without knowing which filter it is.
class Query : public IQuery {
 public:
  /// Questions about `model`, written in `metamodel`, which names the kinds of the answers (DS-351).
  Query(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}

  /// The identities the filter takes from the model.
  std::set<std::string> select(const IFilter& filter) const override;

  /// The questions `ask` understands, with the subject each takes: the manual's one table (DS-436, DS-474).
  static std::vector<model::Question> questions();
  /// The questions as a usage line, as the manual writes it: diagrams · entities [scope] · relations <identity> · ...
  static std::string usage();

  /// Answer `question` about `subject` ("" for the questions that take none); an unknown question, a missing
  /// identity, one the model does not declare, or a subject of a kind the question does not take (DS-436) yields one
  /// answer of kind "error" that says so.
  std::vector<Answer> ask(const std::string& question, const std::string& subject) const override;

  std::vector<Answer> diagrams() const;  ///< every diagram, with its kind
  std::vector<Answer> entities(
      const std::string& scope) const;  ///< the direct members of a scope ("" = the top level); with * after it all it holds
  std::vector<Answer> relations(const std::string& id) const;      ///< every relation touching `id`, with its direction
  std::vector<Answer> artefacts(const std::string& id) const;      ///< the requirements and constraints on `id`, and what verifies them
  std::vector<Answer> behaviours(const std::string& id) const;     ///< the sequences and state machines of a method or class
  std::vector<Answer> failures(const std::string& id) const;       ///< the failures escaping an operation, by where each starts
  std::vector<Answer> shows(const std::string& diagram) const;     ///< the entities a diagram takes, by the filter of its kind
  std::vector<Answer> findings(const std::string& subject) const;  ///< the findings of a file, or of an entity and all it holds ("" = all)
  std::vector<Answer> coverage(const std::string& subject) const;  ///< what is not yet written, as findings asks (DS-216)
  std::vector<Answer> coupling() const;                            ///< what each behaviour touches outside its module, and how

 private:
  bool known(const std::string& id) const;                                                 // an entity, or a scope something is declared in
  std::string refused(const model::Question& question, const std::string& subject) const;  // why the subject is of no kind it takes; ""
  bool within(const model::Location& at, const std::string& id) const;                     // on the entity or on anything it holds
  static bool isFile(const std::string& subject);                                          // a subject that names a file: ends in .ds
  std::vector<std::string> nearTo(const std::string& name) const;                          // identities ending in the name, for a hint
  Answer entityAnswer(const model::Entity& entity, const std::string& detail) const;       // an entity, its kind by name
  static bool endsWith(const std::string& text, const std::string& suffix);                // a finding's file, the one asked
  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

/// The one place that names the query (DS-397), as FilterFactory names the filters: what asks the model a question
/// is handed an IQuery.
class QueryFactory {
 public:
  /// The questions about `model`, written in `metamodel`; both outlive what is handed.
  static std::unique_ptr<IQuery> of(const model::Model& model, const model::Metamodel& metamodel);
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_QUERY_HPP
