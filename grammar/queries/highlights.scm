; DriftSpec highlights — consumed by Neovim (and any tree-sitter editor)

(comment) @comment

[ "package" "module" "extern" "requirement" "constraint" "verification" "interface" "view" "operation"
  "class" "dataclass" "enum" "function" "operator" "sequence" "state" "diagram" "participant" "local" "initial" "on" "return"
  "loop" "alt" "opt" "else" "fail" "attempt" "fails" ] @keyword

(method_modifier) @keyword.modifier
(ownership) @keyword.modifier

(package name: (name) @module)
(module name: (name) @module)
(class name: (name) @type)
(class base: (qualified_name) @type)
(template_param name: (name) @type.parameter)
(template_param bound: (qualified_name) @type)
(enum name: (name) @type)
(enum_value name: (name) @constant)
(requirement id: (name) @constant)
(requirement title: (line_text) @string)
(constraint id: (name) @constant)
(constraint title: (line_text) @string)
(verification id: (name) @constant)
(interface name: (name) @type.definition)
(view name: (name) @type.definition)
(operation name: (name) @function)
(method name: (member_name) @function.method)
(function name: (member_name) @function)
(attribute name: (member_name) @property)
(operator symbol: (operator_symbol) @function.method)
(operator_set (operator_item (operator_symbol) @function.method))
(operator_set (operator_item family: (name) @function.method))
(sequence subject: (qualified_name) @type)
(sequence scenario: (name) @constant)
(local name: (name) @variable)
(local type: (type) @type)
(condition) @string
(message result: (name) @variable)
(argument) @variable
(transition action: (name) @function.method.call)
(state entity: (qualified_name) @type)
(diagram name: (name) @type)
(participant name: (name) @variable)
(participant type: (type) @type)
(parameter name: (name) @variable.parameter)
(type name: (qualified_name) @type)
(default_value) @constant
(number) @number
(string) @string
(message from: (name) @variable)
(message to: (name) @variable)
(message op: (qualified_name) @function.method.call)
(message scenario: (name) @label)
(return value: (name) @variable)
(transition from: (name) @constant)
(transition to: (name) @constant)
(transition event: (name) @function.method.call)
(initial name: (name) @constant)

(field key: (name) @property)
(field value: (line_text) @string)
(field value: (block_text) @string)

(annotation "@" @attribute)
(annotation kind: (name) @attribute)
(annotation target: (qualified_name) @constant)

(guard "[" @punctuation.bracket)
(guard "]" @punctuation.bracket)
(guard_body) @string.special

; rules and definitions: formal, Datalog over the model (DS-94)
[ "rule" "definition" "for" "where" "holds" "never" "if" "not" "is" "in" "arg" "column" ] @keyword
(rule name: (name) @function)
(definition name: (name) @function)
(range variable: (name) @variable)
(relation_atom relation: (name) @function.builtin)
(call_atom definition: (qualified_name) @function.call)
(arg_atom parameter: (name) @variable.parameter)
(compare_atom operator: _ @operator)

(visibility) @operator
[ "->" "::" "=" "<<" ">>" ] @operator
[ "{" "}" "(" ")" "<" ">" ] @punctuation.bracket
[ ":" "," "." ] @punctuation.delimiter
