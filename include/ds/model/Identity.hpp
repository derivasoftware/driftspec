#ifndef DRIFTSPEC_MODEL_IDENTITY_HPP
#define DRIFTSPEC_MODEL_IDENTITY_HPP

#include <cstddef>
#include <string>

namespace ds {
namespace model {

// How identities are written (R4, R10, R15, B1), in one place (DS-124), as three values (DS-184).
// Every package level adds its name with @, everything inside a package joins with ::; a behaviour
// is its subject's identity with ::sequence (and /scenario) or ::state; a template carries its
// arity; a diagram is diagram:NAME. An entity keeps its identity as text: these values wrap it
// where it is taken apart.

class Scope;

/// What an entity is called: p@m::C, p@m::C::run::sequence/refused, diagram:CL_Shop.
class Identity {
 public:
  explicit Identity(std::string text);
  /// A diagram's identity: CL_Shop → diagram:CL_Shop (a name already written so is kept).
  static Identity ofDiagram(const std::string& name);

  const std::string& text() const { return text_; }
  bool empty() const { return text_.empty(); }
  /// The last name: p@m::C → C, app@core → core.
  std::string lastName() const;
  /// Where it is written: p@m::C → p@m, p@m → p@, p@ → "", app@core@ → app@.
  Scope scope() const;
  /// The scope its members are written in, for anything but a package (Scope::inside a package).
  Scope asScope() const;
  /// Written with its path (@ or ::), as an entity, not a bare word.
  bool isQualified() const;
  /// Written from a package (it holds @): absolute, never resolved through scopes.
  bool isAbsolute() const;
  /// It is `outer` or lies inside it: a member (outer::x) or a scenario (outer/x).
  bool within(const Identity& outer) const;
  /// A template's identity with how many parameters it takes (R15): Repository → Repository<1>.
  Identity withArity(std::size_t arity) const;
  /// The name of a diagram's identity: diagram:CL_Shop → CL_Shop (anything else is kept).
  std::string diagramName() const;

  /// Its behaviour: X → X::sequence, or X::sequence/scenario for a scenario written apart.
  Identity sequence(const std::string& scenario = "") const;
  /// Its state machine: X → X::state.
  Identity state() const;
  /// A scenario of this behaviour: X::sequence + s → X::sequence/s.
  Identity withScenario(const std::string& scenario) const;
  /// The behaviour it is or lies in, with its scenario: X::sequence/s::m2 → X::sequence/s; empty when none.
  Identity behaviour() const;
  /// The scenario a behaviour names after /; "" for the default one.
  std::string scenario() const;
  /// The subject of a sequence or of anything in one (its messages, its frames); empty when in none.
  Identity subject() const;
  /// The subject of a state machine X::state. Only for an entity known to be one: a member named
  /// state has the same identity, so the identity alone cannot tell them apart.
  Identity machineSubject() const;

  bool operator==(const Identity& other) const { return text_ == other.text_; }

 private:
  std::string text_;
};

/// Where a name is written: p@ (the system), app@core@ (a package), p@m, p@m::C.
class Scope {
 public:
  explicit Scope(std::string text);
  /// The scope inside a package: p → p@, app@core → app@core@ (a scope already so is kept).
  static Scope inside(const Identity& package);

  const std::string& text() const { return text_; }
  bool empty() const { return text_.empty(); }
  /// `name` written here: p@ + C → p@C, p@m + C → p@m::C, "" + p → p.
  Identity qualify(const std::string& name) const;
  /// The scope outside: p@m::C → p@m, p@m → p@, p@ → "", app@core@ → app@.
  Scope outer() const;
  /// The innermost package scope it lies in: p@m::C → p@, app@core@m → app@core@, C → "".
  Scope package() const;
  /// The box it names: a package scope p@ names the package p; any other scope names itself.
  Identity box() const;
  /// The system's: the outermost package scope (p@).
  bool isSystem() const;

  bool operator==(const Scope& other) const { return text_ == other.text_; }

 private:
  std::string text_;
};

/// A process artefact's name, PREFIX(-PART)*-NUMBER with or without its revision (R5): REQ-00001-2.
class ArtefactName {
 public:
  explicit ArtefactName(std::string text);
  /// Whether a name is written as an artefact's.
  static bool matches(const std::string& text);

  const std::string& text() const { return text_; }
  /// REQ-00001-2 carries its revision, REQ-00001 does not.
  bool hasRevision() const;
  /// Without its revision: REQ-00001-2 → REQ-00001 (anything else is kept).
  ArtefactName withoutRevision() const;

  bool operator==(const ArtefactName& other) const { return text_ == other.text_; }

 private:
  std::string text_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_IDENTITY_HPP
