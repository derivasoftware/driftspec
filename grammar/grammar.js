/**
 * DriftSpec (.ds) — a specification language for design, requirements and
 * behaviour, read per workspace. Consumer-neutral tree-sitter grammar: the
 * same parser serves the engine, the language server and the editors.
 *
 * Static part follows rules R10–R19 (examples/static/static.ds).
 */

const NAME = /[A-Za-z_][A-Za-z0-9_]*(-[A-Za-z0-9_]+)*/;

// operator symbols a class may declare; comparisons come as the family `ordered` (R17)
const OPERATOR_SYMBOLS = ['==', '!=', '+', '-', '*', '/', '%', '[]', '()', '++', '--', '!', '&&', '||', '&', '|', '^', '~', '='];

module.exports = grammar({
  name: 'driftspec',

  extras: $ => [/[ \t\r\n]/, $.comment],

  externals: $ => [$.block_text],

  word: $ => $.name,

  // `a -> b.op() -> r` then `-> op2()`: the second arrow may be a result or the next self call; GLR decides on the `(`
  conflicts: $ => [[$.message]],

  rules: {
    source_file: $ => repeat(choice($.package, $.diagram)),

    comment: $ => token(seq('//', /[^\n]*/)),

    // ---- scopes -----------------------------------------------------------
    // a package holds packages (the HLD nests): written nested, or reopened by its full path
    // (`package app@render {`); every package level adds its name with @ (decided 2026-10-26)
    package: $ => seq('package', repeat(field('prefix', $.package_prefix)), field('name', $.name), '{', repeat($._package_item), '}'),
    // a package is annotated like a class: @requirement for what the box realises, @constraint for what binds it (HLD, 2026-10-26)
    _package_item: $ => choice($.field, $.annotation, $.requirement, $.constraint, $.verification, $.interface, $.package, $.module, $.sequence, $.state),

    module: $ => seq(optional(field('extern', 'extern')), 'module', field('name', $.name), '{', repeat($._module_item), '}'),
    _module_item: $ => choice($.field, $.requirement, $.constraint, $.verification, $.module, $.class, $.enum, $.function, $.sequence, $.state),

    // ---- process artefacts ------------------------------------------------
    // a requirement nested in a requirement refines it; a verification nested in one verifies it (R3)
    requirement: $ => choice(
      seq('requirement', field('id', $.name), ':', field('title', $.line_text)),
      seq('requirement', field('id', $.name), '{', repeat(choice($.field, $.annotation, $.requirement, $.verification, $.constraint)), '}'),
    ),
    verification: $ => seq('verification', field('id', $.name), '{', repeat(choice($.field, $.annotation)), '}'),
    // a constraint is a requirement-like claim; the one-line form carries only its title (R18)
    constraint: $ => choice(
      seq('constraint', field('id', $.name), ':', field('title', $.line_text)),
      seq('constraint', field('id', $.name), '{', repeat(choice($.field, $.annotation)), '}'),
    ),

    // ---- black box --------------------------------------------------------
    // an external interface of a process or a library (2026-11-05). Its kind and the fields of its operations
    // are data, not grammar: `exit 2: refused` is a field with a code, so a new kind changes nothing here
    interface: $ => seq('interface', field('name', $.name), '{', repeat(choice($.field, $.annotation, $.operation)), '}'),
    operation: $ => seq('operation', field('name', $.name), $.parameters, optional(seq('->', field('returns', $.type))),
      optional(seq('{', repeat(choice($.field, $.annotation)), '}'))),

    // ---- white box: static ------------------------------------------------
    class: $ => seq(
      field('kind', choice('class', 'dataclass')),
      field('name', $.name),
      optional(field('params', $.template_params)),
      optional(seq(':', field('base', $.qualified_name), repeat(seq(',', field('base', $.qualified_name))))),
      optional(field('operators', $.operator_set)),
      optional(field('body', $.class_body)),
    ),
    // a function is an operation of the module, outside any class: what a Python module or a C++
    // namespace declares free. Its signature and body are a method's; its scope is the module (2026-10-26)
    function: $ => prec.right(seq(
      optional(field('visibility', $.visibility)),
      'function',
      field('name', $.member_name),
      $.parameters,
      optional(seq('->', field('returns', $.type))),
      optional(field('body', $.member_body)),
    )),
    template_params: $ => seq('<', $.template_param, repeat(seq(',', $.template_param)), '>'),
    template_param: $ => seq(field('name', $.name), optional(seq(':', field('bound', $.qualified_name)))),
    operator_set: $ => seq('<<', $.operator_item, repeat(seq(',', $.operator_item)), '>>'),
    operator_item: $ => choice($.operator_symbol, field('family', $.name)),
    operator_symbol: $ => token(choice(...OPERATOR_SYMBOLS)),
    class_body: $ => seq('{', repeat(choice($.field, $.annotation, $.method, $.attribute, $.operator, $.requirement, $.constraint)), '}'),

    method: $ => prec.right(seq(
      field('visibility', $.visibility),
      repeat(field('modifier', $.method_modifier)),
      field('name', $.member_name),
      $.parameters,
      optional(seq('->', field('returns', $.type))),
      optional(field('body', $.member_body)),
    )),
    method_modifier: $ => choice('abstract', 'static', 'const'),
    attribute: $ => seq(
      field('visibility', $.visibility),
      optional(field('ownership', $.ownership)),
      field('name', $.member_name),
      ':',
      field('type', $.type),
      optional(seq('=', field('default', $.default_value))),
      optional(field('body', $.member_body)),
    ),
    ownership: $ => choice('owns', 'refs', 'shares'),
    // a modifier is a word only in front of a name: `+ abstract: bool` is an attribute named abstract,
    // `+ refs: map<..>` one named refs. The parser decides by what follows; nothing is reserved.
    member_name: $ => choice($.name, alias(choice('abstract', 'static', 'const', 'owns', 'refs', 'shares', 'function'), $.name)),
    operator: $ => prec.right(seq(
      'operator',
      field('symbol', $.operator_symbol),
      $.parameters,
      optional(seq('->', field('returns', $.type))),
      optional(field('body', $.member_body)),
    )),
    member_body: $ => seq('{', repeat(choice($.field, $.annotation, $.contract, $.requirement, $.constraint)), '}'),
    visibility: $ => choice('+', '-', '#'),
    parameters: $ => seq('(', optional(seq($.parameter, repeat(seq(',', $.parameter)))), ')'),
    parameter: $ => seq(field('name', $.name), optional(seq(':', field('type', $.type)))),
    type: $ => seq(field('name', $.qualified_name), optional(field('args', $.type_args))),
    type_args: $ => seq('<', $.type, repeat(seq(',', $.type)), '>'),
    default_value: $ => choice($.number, $.string, 'true', 'false', $.qualified_name),
    contract: $ => seq('[', optional(seq(field('key', $.name), ':')), field('body', $.contract_body), ']'),
    contract_body: $ => /[^\]\n]+/,

    // an enum is annotated like a class (@requirement before its values), decided 2026-10-26
    enum: $ => seq('enum', field('name', $.name), '{', repeat($.annotation), optional(seq($.enum_value, repeat(seq(',', $.enum_value)), optional(','))), '}'),
    enum_value: $ => seq(field('name', $.name), optional(seq('=', field('value', $.number)))),

    // ---- white box: dynamic (block 3) --------------------------------------
    // a behaviour's identity is its subject: the method, class or module it describes;
    // participants are the static context (self, attributes, parameters); only locals are written
    sequence: $ => seq('sequence', field('subject', $.qualified_name), optional(seq('/', field('scenario', $.name))), '{', repeat($._sequence_item), '}'),
    _sequence_item: $ => choice($.field, $.annotation, $.participant, $.local, $.message, $.return, $.loop, $.alt, $.opt),
    participant: $ => seq('participant', field('name', $.name), ':', field('type', $.type)),  // kept for older files: the same as local
    local: $ => seq('local', field('name', $.name), ':', field('type', $.type)),
    message: $ => seq(
      optional(field('from', $.name)),
      '->',
      optional(seq(field('to', $.name), '.')),
      field('op', $.qualified_name),  // `-> tasks::gather(...)`: a function of another module, by its name (2026-10-26)
      $.arguments,
      optional(seq('/', field('scenario', $.name))),  // the scenario of its behaviour this call enters (2026-11-05)
      optional(seq('->', field('result', $.name))),
    ),
    // what the behaviour gives back: a name or a literal, never an expression (2026-11-05)
    return: $ => seq('return', field('value', choice($.name, $.number, $.string, 'true', 'false'))),
    arguments: $ => seq('(', optional(seq($.argument, repeat(seq(',', $.argument)))), ')'),
    argument: $ => seq($.name, repeat(seq('.', $.name))),
    loop: $ => seq('loop', field('condition', $.condition), '{', repeat($._sequence_item), '}'),
    alt: $ => seq('alt', field('condition', $.condition), '{', repeat($._sequence_item), '}', optional(seq('else', '{', repeat(field('otherwise', $._sequence_item)), '}'))),
    opt: $ => seq('opt', field('condition', $.condition), '{', repeat($._sequence_item), '}'),
    condition: $ => /([^{\/\n]|\/[^\/{\n])+/,

    state: $ => seq('state', field('entity', $.qualified_name), '{', repeat(choice($.field, $.annotation, $.initial, $.view, $.transition)), '}'),
    // a state of the navigation of a graphical interface: what it shows, in text, and what binds it (2026-11-05)
    view: $ => seq('view', field('name', $.name), '{', repeat(choice($.field, $.annotation)), '}'),
    initial: $ => seq('initial', field('name', $.name)),
    transition: $ => seq(field('from', $.name), '->', field('to', $.name), 'on', field('event', $.name), optional(field('guard', $.contract)), optional(seq('/', field('action', $.name)))),

    // ---- representations --------------------------------------------------
    diagram: $ => seq('diagram', field('name', $.name), '{', repeat($.field), '}'),

    // ---- common -----------------------------------------------------------
    annotation: $ => seq('@', field('kind', $.name), field('target', $.qualified_name)),
    field: $ => seq(field('key', $.name), optional(field('code', $.number)), ':', optional(field('value', choice($.block_text, $.line_text)))),
    line_text: $ => /([^\/\n]|\/[^\/\n])+/,
    // `pkg@` is one token (no space): `Model` followed by an `@requirement` line is never a package prefix
    qualified_name: $ => seq(repeat(field('package', $.package_prefix)), $.name, repeat(seq('::', choice($.name, '*')))),
    package_prefix: $ => token(seq(NAME, '@')),
    number: $ => /-?[0-9]+(\.[0-9]+)?/,
    string: $ => /"[^"\n]*"/,
    name: $ => NAME,
  },
});
