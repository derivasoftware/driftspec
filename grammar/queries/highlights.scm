; DriftSpec highlights — consumed by Neovim (and any tree-sitter editor)

(comment) @comment

[ "package" "module" "requirement" "verification" "boundary" "operation"
  "class" "sequence" "state" "diagram" "participant" "initial" "on" "return" ] @keyword

(package name: (name) @module)
(module name: (name) @module)
(class name: (name) @type)
(class base: (qualified_name) @type)
(requirement id: (name) @constant)
(verification id: (name) @constant)
(boundary name: (name) @type.definition)
(operation name: (name) @function)
(method name: (name) @function.method)
(attribute name: (name) @property)
(sequence name: (name) @type)
(state entity: (qualified_name) @type)
(diagram name: (name) @type)
(participant name: (name) @variable)
(participant type: (qualified_name) @type)
(parameter name: (name) @variable.parameter)
(type) @type
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
[ "->" "::" ] @operator
[ "{" "}" "(" ")" ] @punctuation.bracket
[ ":" "," "." ] @punctuation.delimiter
