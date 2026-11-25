#ifndef DRIFTSPEC_MODEL_MANUAL_HPP
#define DRIFTSPEC_MODEL_MANUAL_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace model {

/// A command of ds: how it is invoked, what it does, what each exit code says.
struct CommandSpec {
  std::string name;
  std::string synopsis;
  std::string does;
  std::vector<std::pair<int, std::string>> exits;
};

/// A question ds query answers, with the subject it takes (DS-436): the one table the usage, the help and the query's
/// check of a subject read, kept by the manual (DS-474).
struct Question {
  std::string name;
  std::string subject;  ///< as a usage writes it: "" for none, <subject> when it needs one, [subject] when it may take one
  std::string takes;    ///< what it takes, in words: said to a subject of another kind
};

/// The manual ds embeds (DS-79, DS-342): the index, the guides written in doc/manual, the grammar, a kind, a
/// finding code, the commands, the annotations and the codes, each a page written from the metamodel it reads.
/// The commands are the manual's: how each is invoked and what each exit code says, the table every command's
/// help repeats. One is made from the metamodel whoever shows the manual already holds; the metamodel outlives it
/// and never reads it. Its tables are built once, when it is made, and every page reads them.
class Manual {
 public:
  explicit Manual(const Metamodel& metamodel);

  /// The page of a topic: the index for "", a guide, grammar, a kind, a finding code, commands, annotations or
  /// codes; "" for no such topic.
  std::string page(const std::string& topic) const;
  /// Every topic with a page: the guides in the order they are read, the four tables, the kinds and the codes.
  std::vector<std::string> topics() const;
  /// The guides, written as Markdown in doc/manual and embedded when ds is built (DS-272): each by its topic, in
  /// the order they are read. The same text is what ds manual <topic> prints and what the site draws.
  const std::vector<std::pair<std::string, std::string>>& guides() const { return guides_; }
  /// The commands of ds, how each is invoked and what each exit code says.
  const std::vector<CommandSpec>& commands() const { return commands_; }
  /// The command of that name; nullptr when ds has none.
  const CommandSpec* commandOf(const std::string& name) const;
  /// The questions ds query answers, with the subject each takes: the one table (DS-436, DS-474), which the query
  /// reads to answer and the command line to say its help.
  static std::vector<Question> questions();
  /// The questions as a usage line: diagrams · entities [scope] · relations <identity> · ...
  static std::string usage();

 private:
  /// The commands, as the manual says them.
  static std::vector<CommandSpec> buildCommands();
  /// The guides of doc/manual, each joined from the pieces the build embedded (generated: ManualGuides.cpp).
  static std::vector<std::pair<std::string, std::string>> buildGuides();
  /// Every line of `text` after `by`, each ending in a new line.
  static std::string indent(const std::string& text, const std::string& by);
  /// `text` filled with spaces to `width`, or followed by one when it is as long or longer.
  static std::string pad(const std::string& text, std::size_t width);
  /// The kinds as the language writes them, separated by commas.
  std::string names(const std::vector<Kind>& kinds) const;
  /// What a kind is, where it is declared, how it is written, its fields, flags and relations, and its example.
  std::string kindPage(const KindSpec& k) const;
  /// The page with no topic: what ds is, its commands, its guides and the kinds.
  std::string indexPage() const;
  /// How a workspace is written: files, identities, what goes inside what, annotations and behaviour.
  std::string grammarPage() const;

  const Metamodel& metamodel_;
  std::vector<CommandSpec> commands_;
  std::vector<std::pair<std::string, std::string>> guides_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_MANUAL_HPP
