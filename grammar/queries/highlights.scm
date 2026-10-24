; DriftSpec highlights — consumed by Neovim (and any tree-sitter editor)

(comment) @comment

[ "package" "module" "extern" "requirement" "constraint" "verification" "boundary" "operation"
  "class" "dataclass" "enum" "operator" "sequence" "state" "diagram" "participant" "initial" "on" "return" ] @keyword

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
(constraint id: (name) @constant)
(constraint title: (line_text) @string)
(verification id: (name) @constant)
(boundary name: (name) @type.definition)
(operation name: (name) @function)
(method name: (name) @function.method)
(attribute name: (name) @property)
(operator symbol: (operator_symbol) @function.method)
(operator_set (operator_item (operator_symbol) @function.method))
(operator_set (operator_item family: (name) @function.method))
(sequence name: (name) @type)
(state entity: (qualified_name) @type)
(diagram name: (name) @type)
(participant name: (name) @variable)
(participant type: (qualified_name) @type)
(parameter name: (name) @variable.parameter)
(type name: (qualified_name) @type)
(default_value) @constant
(number) @number
(string) @string
(message from: (name) @variable)
(message to: (name) @variable)
(message op: (name) @function.method.call)
(return to: (name) @variable)
(return op: (name) @function.method.call)
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

(contract "[" @punctuation.bracket)
(contract "]" @punctuation.bracket)
(contract key: (name) @keyword.modifier)
(contract_body) @string.special

(visibility) @operator
[ "->" "::" "=" "<<" ">>" ] @operator
[ "{" "}" "(" ")" "<" ">" ] @punctuation.bracket
[ ":" "," "." ] @punctuation.delimiter
