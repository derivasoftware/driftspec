#ifndef DRIFTSPEC_MODEL_IDENTITY_HPP
#define DRIFTSPEC_MODEL_IDENTITY_HPP

#include <cstddef>
#include <string>

namespace ds {
namespace model {

// How identities are written (R4, R10, R15, B1), in one place (DS-124). Every package level adds its
// name with @, everything inside a package joins with ::; a behaviour is its subject's identity with
// ::sequence (and /scenario) or ::state; a template carries its arity; a diagram is diagram:NAME.

/// `name` written in `scope`: p@ + C → p@C, p@m + C → p@m::C, "" + p → p.
std::string qualify(const std::string& scope, const std::string& name);
/// The scope outside `scope`: p@m::C → p@m, p@m → p@, p@ → "", app@core@ → app@.
std::string outer(const std::string& scope);
/// Whether a name is written with its path (@ or ::), as an entity, not a bare word.
bool isQualified(const std::string& name);
/// Whether a name is written from a package (it holds @): absolute, never resolved through scopes.
bool isAbsolute(const std::string& name);
/// The box a scope names: a package scope p@ names the package p; any other scope names itself.
std::string boxOf(const std::string& scope);
/// The innermost package scope a scope lies in: p@m::C → p@, app@core@m → app@core@, C → "".
std::string packageOf(const std::string& scope);
/// The scope inside a package: p → p@, app@core → app@core@ (a scope already so is kept).
std::string scopeIn(const std::string& package);
/// Whether a scope is the system's: the outermost package scope (p@).
bool isSystemScope(const std::string& scope);
/// The last name of an identity: p@m::C → C, app@core → core.
std::string lastName(const std::string& id);
/// Whether `id` is `prefix` or lies inside it: a member (prefix::x) or a scenario (prefix/x).
bool within(const std::string& id, const std::string& prefix);

/// The behaviour of a subject: X::sequence, or X::sequence/scenario for a scenario written apart.
std::string behaviourId(const std::string& subject, const std::string& scenario = "");
/// The state machine of a subject: X::state.
std::string stateId(const std::string& subject);
/// A scenario of a behaviour: X::sequence + s → X::sequence/s.
std::string withScenario(const std::string& behaviour, const std::string& scenario);
/// The subject of a sequence or of anything in one (its messages, its frames); "" when `id` is in none.
std::string subjectOf(const std::string& id);
/// The subject of a state machine X::state. Only for an entity known to be one: a member named state
/// has the same identity, so the identity alone cannot tell them apart.
std::string machineSubjectOf(const std::string& stateMachine);
/// The behaviour `id` is or lies in, with its scenario: X::sequence/s::m2 → X::sequence/s; "" when none.
std::string behaviourOf(const std::string& id);
/// The scenario a behaviour identity names after /; "" for the default one.
std::string scenarioOf(const std::string& id);

/// A template's identity: its name and how many parameters it takes (R15): Repository<1>.
std::string arityId(const std::string& name, std::size_t arity);
/// A diagram's identity: diagram:NAME (a name already written so is kept).
std::string diagramId(const std::string& name);
/// The name of a diagram's identity: diagram:NAME → NAME (anything else is kept).
std::string diagramName(const std::string& id);

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_IDENTITY_HPP
