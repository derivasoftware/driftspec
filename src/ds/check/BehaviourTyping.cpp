#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace ds {
namespace check {

std::string BehaviourTyping::name() const { return "behaviour"; }

std::vector<std::string> BehaviourTyping::needs() const { return {"linker"}; }

BehaviourTyping::BehaviourTyping(Context& context) : context_(&context), model_(&context.model()), resolver_(&context.resolver()) {}

std::vector<model::Finding> BehaviourTyping::run() {
  for (const auto& m : context_->notes().messages) {
    checkMessage(m);
    rememberResult(m);  // in order: a result exists for the messages after it
  }
  checkLocals();
  for (const auto& e : context_->notes().events) checkEvent(e);
  return context_->take();
}

const model::Entity* BehaviourTyping::subjectClass(const model::Entity& sequence, const model::Entity** subject) const {
  const auto field = sequence.fields.find("subject");
  if (field == sequence.fields.end()) return nullptr;
  const std::string id = resolver_->resolve(field->second, sequence.scope);
  const model::Entity* s = id.empty() ? nullptr : model_->find(id);
  if (!s || s->expected) return nullptr;
  *subject = s;
  if (s->kind == model::Kind::Method) return model_->find(s->scope);
  if (s->kind == model::Kind::Class) return s;
  return nullptr;  // a function or an interface's operation has no class: its context is its parameters and its scope
}

void BehaviourTyping::couple(const model::Entity& sequence, const model::Entity& type, bool transient) {
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(sequence, &subject);
  const bool function = subject && (subject->kind == model::Kind::Function || subject->kind == model::Kind::Operation);
  if ((!cls && !function) || type.expected) return;
  const std::string home = cls ? cls->scope : subject->scope;
  if (type.scope == home || (cls && type.id == cls->id)) return;  // inside the module: cohesion, not coupling
  const std::string kind = transient ? "couples-transient" : "couples-structural";
  for (const auto& r : model_->index().from(sequence.id)) {
    if (r.kind == kind && r.to == type.id) return;
  }
  model_->relate({kind, sequence.id, type.id, sequence.location, ""});
}

bool BehaviourTyping::reportOverloads(const model::Entity& seq, const std::string& typeId, const bind::Message& message) {
  // a call that two overloads could be a call of, by its arguments, names none of them: an incoherence (DS-455)
  const std::string name = model::Identity(message.op).lastName();
  const std::vector<std::string> overloads = resolver_->overloadsOf(typeId, name, message.argc);
  if (overloads.size() < 2) return false;
  std::string names;
  for (const auto& o : overloads) names += (names.empty() ? "" : ", ") + o;
  context_->report(context_->metamodel().finding(model::Code::W_NAME_AMBIGUOUS,
                                                 seq.name + " calls " + name + " with " + std::to_string(message.argc) + " argument" +
                                                     (message.argc == 1 ? "" : "s") + ", which " + std::to_string(overloads.size()) +
                                                     " operations of " + typeId + " take: " + names + "; it calls none of them",
                                                 message.location));
  return true;
}

void BehaviourTyping::call(const model::Entity& msg, const model::Entity& op, const bind::Message& message) {
  model_->relate({"calls", msg.id, op.id, message.location, ""});
  // seen from who sends it: the sender written, or the behaviour itself (DS-232)
  context_->checkVisibility(msg.field("from-type").empty() ? msg.scope : msg.field("from-type"), op.id, message.location);
  const int expected = static_cast<int>(op.params.size());
  if (expected != message.argc) {
    context_->report(context_->metamodel().finding(model::Code::E_MSG_ARITY,
                                                   op.id + " takes " + std::to_string(expected) + " parameter" +
                                                       (expected == 1 ? "" : "s") + ", " + std::to_string(message.argc) + " given",
                                                   message.location));
  }
  if (expected == message.argc) checkArguments(msg, op, message);
  const auto returns = op.fields.find("returns");
  if (returns != op.fields.end()) {
    model_->annotate(msg.id, "result_type", returns->second);
    const model::Entity* owner = op.kind == model::Kind::Method ? model_->find(op.scope) : nullptr;
    model_->annotate(msg.id, "result_scope", owner ? owner->scope : op.scope);
  }
  expand(msg, op, message);
}

void BehaviourTyping::expand(const model::Entity& msg, const model::Entity& op, const bind::Message& message) {
  // a message to an operation with a behaviour expands to it (B5): derived, never written; a message
  // that names a scenario expands to that one, and one nobody declares is a finding
  const auto scenario = msg.fields.find("scenario");
  const bool named = scenario != msg.fields.end();
  const std::string id = model::Identity(op.id).sequence(named ? scenario->second : "").text();
  const model::Paths paths(*model_);
  if (const model::Entity* child = model_->find(id)) {
    model_->relate({"expands-to", msg.id, child->id, message.location, ""});
    model_->annotate(msg.id, "expands", child->id);
  } else if (named && paths.hasScenario(model::Identity(op.id).sequence().text(), scenario->second)) {
    // a named branch of the one behaviour: the call enters that path of it
    model_->relate({"expands-to", msg.id, model::Identity(op.id).sequence().text(), message.location, ""});
    model_->annotate(msg.id, "expands", model::Identity(op.id).sequence().text());
    model_->annotate(msg.id, "expands-path", scenario->second);
  } else if (named) {
    context_->report(context_->metamodel().finding(model::Code::W_SCENARIO_UNDECLARED,
                                                   op.id + " has no scenario '" + scenario->second + "'", message.location));
  }
}

void BehaviourTyping::checkReturn(const bind::Message& message) {
  const model::Entity* seq = model_->find(context_->current(message.sequence));
  const model::Entity* msg = model_->find(context_->current(message.message));
  if (!seq || !msg) return;
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(*seq, &subject);
  if (!subject || subject->kind == model::Kind::Class || subject->kind == model::Kind::Operation) return;
  const auto returns = subject->fields.find("returns");
  if (returns == subject->fields.end()) return;  // declares no return: nothing to check against
  const std::string value = msg->fields.count("result") ? msg->fields.at("result") : "";
  if (value.empty()) {
    // a bare return ends a branch; it gives nothing back, which only an operation returning none may do at its end
    return;
  }
  const std::string home = cls ? cls->scope : subject->scope;
  std::string given;
  bool mismatch = false;
  if (value.front() == '"' || value == "true" || value == "false" || value.front() == '-' ||
      std::isdigit(static_cast<unsigned char>(value.front()))) {
    given = "a literal";
    mismatch = !compatible({"", home}, {returns->second, home}, true);
  } else {
    std::pair<std::string, std::string> named;
    bool transient = false;
    // a name the context does not know is a local of the prose: absence is not an error (B4)
    if (!typeOfName(*seq, cls, *subject, value, &named, &transient) || named.first.empty()) return;
    given = named.first;
    mismatch = !compatible(named, {returns->second, home});
  }
  if (mismatch) {
    context_->report(context_->metamodel().finding(
        model::Code::W_RETURN_TYPE, subject->id + " returns " + returns->second + ", and '" + value + "' is " + given, message.location));
  }
}

void BehaviourTyping::checkMessage(const bind::Message& message) {
  if (message.returns) {
    checkReturn(message);
    return;
  }
  const model::Entity* seq = model_->find(context_->current(message.sequence));
  const model::Entity* msg = model_->find(context_->current(message.message));
  if (!seq || !msg) return;
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(*seq, &subject);
  const bool function = subject && (subject->kind == model::Kind::Function || subject->kind == model::Kind::Operation);
  // a use case, a package or a module as subject: a scenario written at its cut, between the boxes it sees (DS-232)
  const bool box =
      subject && (subject->kind == model::Kind::Package || subject->kind == model::Kind::Module || subject->kind == model::Kind::UseCase);
  if (!cls && !function && !box) return;  // the subject waits for its declaration (R6)
  // where its names resolve from: a class's module, a box's inside, the scope a function or a use case is declared in
  std::string home = cls ? cls->scope : subject->scope;
  if (subject->kind == model::Kind::Package) home = subject->id + "@";
  if (subject->kind == model::Kind::Module) home = subject->id;
  typeSender(*seq, cls, *subject, *msg, home);
  // a call with no target from a function, or one the class does not declare, is a function of the
  // module, resolved outward like any name: a free function calls its neighbours (2026-10-26)
  if (!box && message.to == "self" && (function || !resolver_->member(cls->id, message.op))) {
    const std::string id = resolver_->resolve(message.op, home);
    const model::Entity* target = id.empty() ? nullptr : model_->find(id);
    if (target && target->kind == model::Kind::Function) {
      model_->annotate(msg->id, "type", target->scope);
      couple(*seq, *target, true);  // a function of another module couples like a local
      call(*msg, *target, message);
      return;
    }
    if (callStatic(*seq, *msg, message, home)) return;
    // a function nobody designs (subprocess::run, a library's helper) is text: no relation, no finding; so is a
    // qualified call from a class's behaviour. Only a bare call on self still asks the class (DS-18)
    if (function || message.op.find("::") != std::string::npos) return;
  }
  // the participant: self, or a name of the behaviour's context (B3)
  std::string typeId;
  bool transient = false;
  if (message.to == "self") {
    typeId = cls ? cls->id : subject->id;
  } else {
    std::pair<std::string, std::string> value;
    if (typeOfName(*seq, cls, *subject, message.to, &value, &transient)) {
      typeId = value.first.empty() ? "" : resolver_->resolveTypeText(value.first, value.second);
    } else {
      typeId = namedEntity(message.to, home);  // a box, an interface or a class named as such is the participant (DS-232)
      if (typeId.empty()) return;              // a receiver nobody designs is text: no relation, no finding (DS-18)
    }
  }
  if (typeId.empty()) return;  // a primitive or an unknown spelling: nothing to check
  const model::Entity* type = model_->find(typeId);
  if (!type || type->expected) return;  // reported once the type is declared (R6)
  model_->annotate(msg->id, "type", typeId);
  if (message.to != "self") couple(*seq, *type, transient);
  // a message written between two participants says that the one uses the other (DS-230): the sender written,
  // or the box a scenario is of; whoever asks for a use case is outside the design
  std::string sender = msg->field("from-type");
  if (sender.empty() && box && subject->kind != model::Kind::UseCase) sender = subject->id;
  if (!sender.empty() && sender != typeId) {
    bool said = false;
    for (const auto& r : model_->index().fromOfKind(sender, "sends")) said = said || r.to == typeId;
    if (!said) model_->relate({"sends", sender, typeId, message.location, ""});
  }
  const std::string opId = typeId + "::" + message.op;
  const model::Entity* op =
      resolver_->member(typeId, message.op, message.argc);  // its own, or inherited; of its overloads, the one it can call
  const bool aBox = type->kind == model::Kind::Package || type->kind == model::Kind::Module;
  if (aBox) op = offered(*type, message.op);  // a message to a box goes down to the operation the box offers under that name
  if (!op && aBox) {
    // and when it offers none the message is a label: the level below is not written yet, which is coverage, no finding
    context_->report(context_->metamodel().finding(
        model::Code::W_MESSAGE_NO_OPERATION,
        seq->name + " sends " + message.op + " to " + typeId + ", which offers no operation of that name", message.location));
    return;
  }
  if (!op && reportOverloads(*seq, typeId, message)) return;
  if (!op) {
    context_->report(context_->metamodel().finding(
        model::Code::W_MSG_UNDECLARED, seq->name + " calls " + opId + ", which " + typeId + " does not declare", message.location));
    return;
  }
  call(*msg, *op, message);
}

std::string BehaviourTyping::namedEntity(const std::string& name, const std::string& home) const {
  // a participant may be named as what it is: a box, an interface or a class seen from the behaviour (DS-232)
  const std::string id = resolver_->resolve(name, home);
  const model::Entity* e = id.empty() ? nullptr : model_->find(id);
  if (!e || e->expected) return "";
  const bool named = e->kind == model::Kind::Package || e->kind == model::Kind::Module || e->kind == model::Kind::Interface ||
                     e->kind == model::Kind::Class;
  return named ? id : "";
}

void BehaviourTyping::typeSender(const model::Entity& seq, const model::Entity* cls, const model::Entity& subject, const model::Entity& msg,
                                 const std::string& home) {
  // who sends is the participant written before the arrow, typed like the receiver; with none, the subject (DS-232).
  // A sender nobody designs is text (DS-18)
  const std::string sender = msg.field("from");
  if (sender.empty() || sender == "self") return;
  std::pair<std::string, std::string> value;
  bool transient = false;
  std::string type;
  if (typeOfName(seq, cls, subject, sender, &value, &transient)) {
    type = value.first.empty() ? "" : resolver_->resolveTypeText(value.first, value.second);
  } else {
    type = namedEntity(sender, home);
  }
  if (!type.empty()) model_->annotate(msg.id, "from-type", type);
}

const model::Entity* BehaviourTyping::offered(const model::Entity& box, const std::string& op) const {
  // a module offers its functions; a package, the operations of its interfaces: one of that name, or none
  if (box.kind == model::Kind::Module) {
    const model::Entity* function = model_->find(box.id + "::" + op);
    return function && function->kind == model::Kind::Function ? function : nullptr;
  }
  const model::Entity* found = nullptr;
  int count = 0;
  for (const auto& pair : model_->entities()) {
    const model::Entity& interface = pair.second;
    if (interface.kind != model::Kind::Interface || interface.expected || model::Scope(interface.scope).box().text() != box.id) continue;
    if (const model::Entity* operation = resolver_->member(interface.id, op)) {
      found = operation;
      ++count;
    }
  }
  return count == 1 ? found : nullptr;
}

bool BehaviourTyping::callStatic(const model::Entity& seq, const model::Entity& msg, const bind::Message& message,
                                 const std::string& home) {
  // `-> diff::Diff::compare(a, b)`: a qualified name with no participant that names a class's operation
  // calls it on the class itself, as a function of another module is called (DS-60)
  const auto at = message.op.rfind("::");
  if (at == std::string::npos) return false;
  const std::string typeId = resolver_->resolve(message.op.substr(0, at), home);
  const model::Entity* type = typeId.empty() ? nullptr : model_->find(typeId);
  if (!type || type->kind != model::Kind::Class) return false;
  const model::Entity* op = resolver_->member(typeId, message.op.substr(at + 2), message.argc);
  if (!op) return reportOverloads(seq, typeId, message);  // two it could be a call of: said, and handled
  model_->annotate(msg.id, "type", typeId);
  couple(seq, *type, true);  // the class is reached for one call, like a local
  if (!op->has("static")) {
    context_->report(context_->metamodel().finding(
        model::Code::W_MSG_NOT_STATIC, seq.name + " calls " + op->id + " on its class, and it is not static: it needs an instance",
        message.location));
    return true;
  }
  call(msg, *op, message);
  return true;
}

bool BehaviourTyping::typeOfName(const model::Entity& sequence, const model::Entity* cls, const model::Entity& subject,
                                 const std::string& name, std::pair<std::string, std::string>* value, bool* transient) const {
  // as the code scopes a name: a local the behaviour declares hides a member of its name, then a
  // parameter, then an attribute of the class (a method is no value a message is sent to), then the
  // result of an earlier message (one named like an attribute is stored in that attribute)
  const auto local = sequence.fields.find("local." + name);
  if (local != sequence.fields.end()) {
    *value = {local->second, sequence.scope};
    *transient = true;
    return true;
  }
  const std::string home = cls ? cls->scope : subject.scope;
  for (const auto& p : subject.params) {
    if (p.name != name) continue;
    *value = {p.type, home};
    return true;
  }
  const model::Entity* attribute = cls ? resolver_->member(cls->id, name) : nullptr;
  if (attribute && attribute->kind == model::Kind::Attribute) {
    const auto type = attribute->fields.find("type");
    *value = {type == attribute->fields.end() ? "" : type->second, cls->scope};
    return true;
  }
  const auto results = results_.find(sequence.id);  // a result named like an attribute is stored in it
  if (results != results_.end()) {
    const auto result = results->second.find(name);
    if (result != results->second.end()) {
      *value = result->second;
      *transient = true;
      return true;
    }
  }
  return false;
}

// ds knows no language, and so no type of its own (DS-18): a literal differs only from a designed class (a "x"
// passed where a Pool is expected); two designed types differ unless one derives from the other; text on either
// side (int, str, std::string, Optional[str], anything nobody designs) never differs
bool BehaviourTyping::compatible(const std::pair<std::string, std::string>& argument, const std::pair<std::string, std::string>& parameter,
                                 bool literal) const {
  const std::string parameterId = resolver_->resolveTypeText(parameter.first, parameter.second);
  const model::Entity* p = parameterId.empty() ? nullptr : model_->find(parameterId);
  if (!p || p->expected) return true;
  if (literal) return false;
  if (argument.first == parameter.first) return true;
  const std::string argumentId = resolver_->resolveTypeText(argument.first, argument.second);
  const model::Entity* a = argumentId.empty() ? nullptr : model_->find(argumentId);
  if (!a || a->expected) return true;
  return resolver_->derives(argumentId, parameterId);
}

void BehaviourTyping::checkArguments(const model::Entity& msg, const model::Entity& op, const bind::Message& message) {
  if (op.params.empty() || msg.args.empty()) return;
  const model::Entity* seq = model_->find(context_->current(message.sequence));
  const model::Entity* subject = nullptr;
  const model::Entity* cls = seq ? subjectClass(*seq, &subject) : nullptr;
  if (!subject) return;
  const std::vector<model::Param>& parameters = op.params;
  const std::vector<std::string>& arguments = msg.args;
  const model::Entity* owner = op.kind == model::Kind::Method ? model_->find(op.scope) : nullptr;
  const std::string scope = owner ? owner->scope : op.scope;
  for (size_t i = 0; i < parameters.size() && i < arguments.size(); ++i) {
    if (parameters[i].type.empty()) continue;  // a parameter with no type
    std::pair<std::string, std::string> argument;
    bool transient = false;
    const std::string& written = arguments[i];
    const bool literal = written.front() == '"' || std::isdigit(static_cast<unsigned char>(written.front())) || written.front() == '-' ||
                         written == "true" || written == "false";
    if (literal) {
      argument = {"", scope};  // a literal, of no type ds knows
    } else if (!typeOfName(*seq, cls, *subject, written, &argument, &transient) || argument.first.empty()) {
      continue;
    }
    const std::pair<std::string, std::string> parameter{parameters[i].type, scope};
    if (compatible(argument, parameter, literal)) continue;
    context_->report(context_->metamodel().finding(model::Code::W_ARG_TYPE,
                                                   op.id + " takes " + parameters[i].name + ": " + parameters[i].type + ", and '" +
                                                       arguments[i] + "' is " + (literal ? std::string("a literal") : argument.first),
                                                   message.location));
  }
}

void BehaviourTyping::rememberResult(const bind::Message& message) {
  if (message.returns) return;
  const model::Entity* seq = model_->find(context_->current(message.sequence));
  const model::Entity* msg = model_->find(context_->current(message.message));
  if (!seq || !msg) return;
  const auto result = msg->fields.find("result");
  if (result == msg->fields.end()) return;
  const auto type = msg->fields.find("result_type");
  const auto scope = msg->fields.find("result_scope");
  // a call that resolved to nothing leaves a value of unknown type: the name exists, the finding was given
  results_[seq->id][result->second] = {type == msg->fields.end() ? "" : type->second, scope == msg->fields.end() ? "" : scope->second};
}

void BehaviourTyping::checkLocals() {
  std::vector<std::pair<std::string, std::string>> locals;
  for (const auto& pair : model_->entities()) {
    if (pair.second.kind != model::Kind::Sequence) continue;
    for (const auto& f : pair.second.fields) {
      if (f.first.rfind("local.", 0) == 0) locals.push_back({pair.first, f.second});
    }
  }
  for (const auto& l : locals) {
    const model::Entity* seq = model_->find(l.first);
    const std::string typeId = resolver_->resolveTypeText(l.second, seq->scope);
    const model::Entity* type = typeId.empty() ? nullptr : model_->find(typeId);
    if (type) couple(*seq, *type, true);
  }
}

void BehaviourTyping::checkEvent(const bind::Event& event) {
  const std::string type = resolver_->resolve(event.entity, event.scope);
  const model::Entity* entity = type.empty() ? nullptr : model_->find(type);
  if (!entity || entity->expected) return;
  const std::string op = type + "::" + event.event;
  if (const model::Entity* target = model_->find(op)) {
    model_->relate({event.action ? "does" : "on", context_->current(event.state), target->id, event.location, ""});
    return;
  }
  if (entity->kind == model::Kind::Interface) return;  // a jump with nothing behind it is a label of the navigation
  if (event.action) {
    context_->report(context_->metamodel().finding(
        model::Code::W_ACTION_UNDECLARED, "state " + event.state + " runs '" + event.event + "', which is not an operation of " + type,
        event.location));
    return;
  }
  context_->report(context_->metamodel().finding(
      model::Code::W_EVENT_UNDECLARED, "state " + event.state + " reacts to '" + event.event + "', which is not an operation of " + type,
      event.location));
}

}  // namespace check
}  // namespace ds
