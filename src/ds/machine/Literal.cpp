#include "ds/machine/Literal.hpp"

namespace ds {
namespace machine {

Literal Literal::read(std::string text) {
  Literal l;
  if (!text.empty() && text[0] == '!') {  // `not`, kept as a leading !; a name never starts with one
    l.negated = true;
    text.erase(0, 1);
  }
  const auto differs = text.find("!=");
  const auto equals = text.find('=');
  if (differs != std::string::npos) {
    l.name = text.substr(0, differs);
    l.op = "!=";
    l.value = text.substr(differs + 2);
  } else if (equals != std::string::npos) {
    l.name = text.substr(0, equals);
    l.op = "=";
    l.value = text.substr(equals + 1);
  } else {
    l.name = text;
  }
  return l;
}

std::string Literal::written() const { return (negated ? "not " : "") + name + (op.empty() ? "" : " " + op + " " + value); }

}  // namespace machine
}  // namespace ds
