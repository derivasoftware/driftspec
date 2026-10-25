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
    package: $ => seq('package', field('name', $.name), '{', repeat($._package_item), '}'),
    _package_item: $ => choice($.field, $.requirement, $.constraint, $.verification, $.boundary, $.module),

    module: $ => seq(optional(field('extern', 'extern')), 'module', field('name', $.name), '{', repeat($._module_item), '}'),
    _module_item: $ => choice($.field, $.requirement, $.constraint, $.verification, $.module, $.class, $.enum, $.sequence, $.state),

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
    boundary: $ => seq('boundary', field('name', $.name), '{', repeat(choice($.field, $.annotation, $.operation)), '}'),
    operation: $ => seq('operation', field('name', $.name), $.parameters, optional(seq('->', field('returns', $.type)))),

    // ---- white box: static ------------------------------------------------
    class: $ => seq(
      field('kind', choice('class', 'dataclass')),
      field('name', $.name),
      optional(field('params', $.template_params)),
      optional(seq(':', field('base', $.qualified_name), repeat(seq(',', field('base', $.qualified_name))))),
      optional(field('operators', $.operator_set)),
      optional(field('body', $.class_body)),
    ),
    template_params: $ => seq('<', $.template_param, repeat(seq(',', $.template_param)), '>'),
    template_param: $ => seq(field('name', $.name), optional(seq(':', field('bound', $.qualified_name)))),
    operator_set: $ => seq('<<', $.operator_item, repeat(seq(',', $.operator_item)), '>>'),
    operator_item: $ => choice($.operator_symbol, field('family', $.name)),
    operator_symbol: $ => token(choice(...OPERATOR_SYMBOLS)),
    class_body: $ => seq('{', repeat(choice($.field, $.annotation, $.method, $.attribute, $.operator, $.requirement, $.constraint)), '}'),

    method: $ => prec.right(seq(
      field('visibility', $.visibility),
      repeat(field('modifier', $.method_modifier)),
      field('name', $.name),
      $.parameters,
      optional(seq('->', field('returns', $.type))),
      repeat($.contract),
      optional(field('body', $.member_body)),
    )),
    method_modifier: $ => choice('abstract', 'static', 'const'),
    attribute: $ => seq(
      field('visibility', $.visibility),
      optional(field('ownership', $.ownership)),
      field('name', $.name),
      ':',
      field('type', $.type),
      optional(seq('=', field('default', $.default_value))),
      optional(field('body', $.member_body)),
    ),
    ownership: $ => choice('owns', 'refs', 'shares'),
    operator: $ => prec.right(seq(
      'operator',
      field('symbol', $.operator_symbol),
      $.parameters,
      optional(seq('->', field('returns', $.type))),
      repeat($.contract),
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

    enum: $ => seq('enum', field('name', $.name), '{', optional(seq($.enum_value, repeat(seq(',', $.enum_value)), optional(','))), '}'),
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
      field('op', $.name),
      $.arguments,
      optional(seq('->', field('result', $.name))),
    ),
    return: $ => seq('return', field('to', $.name), '.', field('op', $.name), '->', field('value', $.name)),
    arguments: $ => seq('(', optional(seq($.argument, repeat(seq(',', $.argument)))), ')'),
    argument: $ => seq($.name, repeat(seq('.', $.name))),
    loop: $ => seq('loop', field('condition', $.condition), '{', repeat($._sequence_item), '}'),
    alt: $ => seq('alt', field('condition', $.condition), '{', repeat($._sequence_item), '}', optional(seq('else', '{', repeat(field('otherwise', $._sequence_item)), '}'))),
    opt: $ => seq('opt', field('condition', $.condition), '{', repeat($._sequence_item), '}'),
    condition: $ => /([^{\/\n]|\/[^\/{\n])+/,

    state: $ => seq('state', field('entity', $.qualified_name), '{', repeat(choice($.field, $.annotation, $.initial, $.transition)), '}'),
    initial: $ => seq('initial', field('name', $.name)),
    transition: $ => seq(field('from', $.name), '->', field('to', $.name), 'on', field('event', $.name), optional(field('guard', $.contract)), optional(seq('/', field('action', $.name)))),

    // ---- representations --------------------------------------------------
    diagram: $ => seq('diagram', field('name', $.name), '{', repeat($.field), '}'),

    // ---- common -----------------------------------------------------------
    annotation: $ => seq('@', field('kind', $.name), field('target', $.qualified_name)),
    field: $ => seq(field('key', $.name), ':', optional(field('value', choice($.block_text, $.line_text)))),
    line_text: $ => /([^\/\n]|\/[^\/\n])+/,
    qualified_name: $ => seq($.name, repeat(seq('::', choice($.name, '*')))),
    number: $ => /-?[0-9]+(\.[0-9]+)?/,
    string: $ => /"[^"\n]*"/,
    name: $ => NAME,
  },
});
