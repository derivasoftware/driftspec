#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

std::string BehaviourTyping::name() const { return "behaviour"; }

std::vector<std::string> BehaviourTyping::needs() const { return {"linker"}; }

std::vector<model::Finding> BehaviourTyping::run(Context& out_context) {
  context_ = &out_context;
  model_ = &out_context.model();
  resolver_ = &out_context.resolver();
  for (const auto& m : context_->binder().messages()) {
    checkMessage(m);
    rememberResult(m);  // in order: a result exists for the messages after it
  }
  checkLocals();
  for (const auto& e : context_->binder().events()) checkEvent(e);
  return out_context.take();
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
  if ((!cls && !function) || type.expected || type.has("extern")) return;
  const std::string home = cls ? cls->scope : subject->scope;
  if (type.scope == home || (cls && type.id == cls->id)) return;  // inside the module: cohesion, not coupling
  const std::string kind = transient ? "couples-transient" : "couples-structural";
  for (const auto& r : model_->index().from(sequence.id)) {
    if (r.kind == kind && r.to == type.id) return;
  }
  model_->relate({kind, sequence.id, type.id, sequence.location, ""});
}

void BehaviourTyping::call(model::Entity& out_msg, const model::Entity& op, const bind::Message& message) {
  model_->relate({"calls", out_msg.id, op.id, message.location, ""});
  // seen from who sends it: the sender written, or the behaviour itself (DS-232)
  context_->checkVisibility(out_msg.field("from-type").empty() ? out_msg.scope : out_msg.field("from-type"), op.id, message.location);
  const int expected = static_cast<int>(op.params.size());
  if (expected != message.argc) {
    context_->report({"E-MSG-ARITY", "error",
                      op.id + " takes " + std::to_string(expected) + " parameter" + (expected == 1 ? "" : "s") + ", " +
                          std::to_string(message.argc) + " given",
                      message.location});
  }
  if (expected == message.argc) checkArguments(out_msg, op, message);
  const auto returns = op.fields.find("returns");
  const auto result = out_msg.fields.find("result");
  if (returns != op.fields.end()) {
    if (result != out_msg.fields.end() && primitiveFamily(returns->second) == 4) {
      context_->report(
          {"W-RESULT-NONE", "warning", op.id + " returns nothing, so '" + result->second + "' holds no value", message.location});
    }
    out_msg.fields["result_type"] = returns->second;
    const model::Entity* owner = op.kind == model::Kind::Method ? model_->find(op.scope) : nullptr;
    out_msg.fields["result_scope"] = owner ? owner->scope : op.scope;
  }
  expand(out_msg, op, message);
}

void BehaviourTyping::expand(model::Entity& out_msg, const model::Entity& op, const bind::Message& message) {
  // a message to an operation with a behaviour expands to it (B5): derived, never written; a message
  // that names a scenario expands to that one, and one nobody declares is a finding
  const auto scenario = out_msg.fields.find("scenario");
  const bool named = scenario != out_msg.fields.end();
  const std::string id = model::Identity(op.id).sequence(named ? scenario->second : "").text();
  if (const model::Entity* child = model_->find(id)) {
    model_->relate({"expands-to", out_msg.id, child->id, message.location, ""});
    out_msg.fields["expands"] = child->id;
  } else if (named && model::hasScenario(*model_, model::Identity(op.id).sequence().text(), scenario->second)) {
    // a named branch of the one behaviour: the call enters that path of it
    model_->relate({"expands-to", out_msg.id, model::Identity(op.id).sequence().text(), message.location, ""});
    out_msg.fields["expands"] = model::Identity(op.id).sequence().text();
    out_msg.fields["expands-path"] = scenario->second;
  } else if (named) {
    context_->report({"W-SCENARIO-UNDECLARED", "warning", op.id + " has no scenario '" + scenario->second + "'", message.location});
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
    given = value.front() == '"' ? "string" : (value == "true" || value == "false") ? "bool" : "int";
    mismatch = !compatible({given, home}, {returns->second, home});
  } else {
    std::pair<std::string, std::string> named;
    bool transient = false;
    // a name the context does not know is a local of the prose: absence is not an error (B4)
    if (!typeOfName(*seq, cls, *subject, value, &named, &transient) || named.first.empty()) return;
    given = named.first;
    mismatch = !compatible(named, {returns->second, home});
  }
  if (mismatch) {
    context_->report(
        {"W-RETURN-TYPE", "warning", subject->id + " returns " + returns->second + ", and '" + value + "' is " + given, message.location});
  }
}

void BehaviourTyping::checkMessage(const bind::Message& message) {
  if (message.returns) {
    checkReturn(message);
    return;
  }
  const model::Entity* seq = model_->find(context_->current(message.sequence));
  model::Entity* msg = model_->find(context_->current(message.message));
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
  if (!typeSender(*seq, cls, *subject, *msg, home, message.location)) return;
  // a call with no target from a function, or one the class does not declare, is a function of the
  // module, resolved outward like any name: a free function calls its neighbours (2026-10-26)
  if (!box && message.to == "self" && (function || !resolver_->member(cls->id, message.op))) {
    const std::string id = resolver_->resolve(message.op, home);
    const model::Entity* target = id.empty() ? nullptr : model_->find(id);
    if (target && target->kind == model::Kind::Function) {
      msg->fields["type"] = target->scope;
      couple(*seq, *target, true);  // a function of another module couples like a local
      call(*msg, *target, message);
      return;
    }
    if (callStatic(*seq, *msg, message, home)) return;
    if (function) {
      context_->report({"W-MSG-UNDECLARED", "warning", seq->name + " calls " + message.op + ", which no function of " + home + " declares",
                        message.location});
      return;
    }
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
      if (typeId.empty()) {
        const std::string context = cls ? "an attribute of " + cls->id + ", a parameter of " : "a parameter of ";
        context_->report({"W-PARTICIPANT-UNDECLARED", "warning",
                          "'" + message.to + "' is neither " + context + subject->name + ", a local of " + seq->name +
                              " nor the result of an earlier message",
                          message.location});
        return;
      }
    }
  }
  if (typeId.empty()) return;  // a primitive or an unknown spelling: nothing to check
  const model::Entity* type = model_->find(typeId);
  if (!type || type->expected) return;  // reported once the type is declared (R6)
  msg->fields["type"] = typeId;
  if (message.to != "self") couple(*seq, *type, transient);
  const std::string opId = typeId + "::" + message.op;
  const model::Entity* op = resolver_->member(typeId, message.op);  // its own, or inherited
  const bool aBox = type->kind == model::Kind::Package || type->kind == model::Kind::Module;
  if (aBox) op = offered(*type, message.op);  // a message to a box goes down to the operation the box offers under that name
  if (!op && aBox) {
    // and when it offers none the message is a label: the level below is not written yet, which is coverage, no finding
    context_->report({"W-MESSAGE-NO-OPERATION", "coverage",
                      seq->name + " sends " + message.op + " to " + typeId + ", which offers no operation of that name", message.location});
    return;
  }
  if (!op) {
    context_->report(
        {"W-MSG-UNDECLARED", "warning", seq->name + " calls " + opId + ", which " + typeId + " does not declare", message.location});
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

bool BehaviourTyping::typeSender(const model::Entity& seq, const model::Entity* cls, const model::Entity& subject, model::Entity& out_msg,
                                 const std::string& home, const model::Location& location) {
  // who sends is the participant written before the arrow, typed like the receiver; with none, the subject (DS-232)
  const std::string sender = out_msg.field("from");
  if (sender.empty() || sender == "self") return true;
  std::pair<std::string, std::string> value;
  bool transient = false;
  std::string type;
  if (typeOfName(seq, cls, subject, sender, &value, &transient)) {
    type = value.first.empty() ? "" : resolver_->resolveTypeText(value.first, value.second);
  } else {
    type = namedEntity(sender, home);
    if (type.empty()) {
      context_->report({"W-PARTICIPANT-UNDECLARED", "warning",
                        "'" + sender + "' sends a message in " + seq.name +
                            ", and it is neither a value of its context nor a box, an interface or a class it sees",
                        location});
      return false;
    }
  }
  if (!type.empty()) out_msg.fields["from-type"] = type;
  return true;
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

bool BehaviourTyping::callStatic(const model::Entity& seq, model::Entity& out_msg, const bind::Message& message, const std::string& home) {
  // `-> diff::Diff::compare(a, b)`: a qualified name with no participant that names a class's operation
  // calls it on the class itself, as a function of another module is called (DS-60)
  const auto at = message.op.rfind("::");
  if (at == std::string::npos) return false;
  const std::string typeId = resolver_->resolve(message.op.substr(0, at), home);
  const model::Entity* type = typeId.empty() ? nullptr : model_->find(typeId);
  if (!type || type->kind != model::Kind::Class) return false;
  const model::Entity* op = resolver_->member(typeId, message.op.substr(at + 2));
  if (!op) return false;
  out_msg.fields["type"] = typeId;
  couple(seq, *type, true);  // the class is reached for one call, like a local
  if (!op->has("static")) {
    context_->report({"W-MSG-NOT-STATIC", "warning",
                      seq.name + " calls " + op->id + " on its class, and it is not static: it needs an instance", message.location});
    return true;
  }
  call(out_msg, *op, message);
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

bool BehaviourTyping::compatible(const std::pair<std::string, std::string>& argument,
                                 const std::pair<std::string, std::string>& parameter) const {
  if (argument.first == parameter.first) return true;
  const std::string argumentId = resolver_->resolveTypeText(argument.first, argument.second);
  const std::string parameterId = resolver_->resolveTypeText(parameter.first, parameter.second);
  const model::Entity* a = argumentId.empty() ? nullptr : model_->find(argumentId);
  const model::Entity* p = parameterId.empty() ? nullptr : model_->find(parameterId);
  // what is not declared, or lives outside the workspace, has no obligations (R6, R11)
  if ((a && (a->expected || a->has("extern"))) || (p && (p->expected || p->has("extern")))) return true;
  if (a && p) return resolver_->derives(argumentId, parameterId);
  const int af = primitiveFamily(argument.first);
  const int pf = primitiveFamily(parameter.first);
  if (a || p) return !(a ? pf : af);
  return !af || !pf || af == pf;
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
    if (written.front() == '"') {
      argument = {"string", scope};  // a literal has the type it is written in
    } else if (std::isdigit(static_cast<unsigned char>(written.front())) || written.front() == '-') {
      argument = {"int", scope};
    } else if (written == "true" || written == "false") {
      argument = {"bool", scope};
    } else if (!typeOfName(*seq, cls, *subject, written, &argument, &transient) || argument.first.empty()) {
      continue;
    }
    const std::pair<std::string, std::string> parameter{parameters[i].type, scope};
    if (compatible(argument, parameter)) continue;
    context_->report(
        {"W-ARG-TYPE", "warning",
         op.id + " takes " + parameters[i].name + ": " + parameters[i].type + ", and '" + arguments[i] + "' is " + argument.first,
         message.location});
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
    context_->report({"W-ACTION-UNDECLARED", "warning",
                      "state " + event.state + " runs '" + event.event + "', which is not an operation of " + type, event.location});
    return;
  }
  context_->report({"W-EVENT-UNDECLARED", "warning",
                    "state " + event.state + " reacts to '" + event.event + "', which is not an operation of " + type, event.location});
}

}  // namespace check
}  // namespace ds
