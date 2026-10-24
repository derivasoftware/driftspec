/**
 * DriftSpec (.ds) — a specification language for design, requirements and
 * behaviour, read per workspace. Consumer-neutral tree-sitter grammar: the
 * same parser serves the engine, the language server and the editors.
 */

const NAME = /[A-Za-z_][A-Za-z0-9_]*(-[A-Za-z0-9_]+)*/;

module.exports = grammar({
  name: 'driftspec',

  extras: $ => [/[ \t\r\n]/, $.comment],

  externals: $ => [$.block_text],

  word: $ => $.name,

  rules: {
    source_file: $ => repeat(choice($.package, $.diagram)),

    comment: $ => token(seq('//', /[^\n]*/)),

    // ---- scopes -----------------------------------------------------------
    package: $ => seq('package', field('name', $.name), '{', repeat($._package_item), '}'),
    _package_item: $ => choice($.field, $.requirement, $.verification, $.boundary, $.module),

    module: $ => seq('module', field('name', $.name), '{', repeat($._module_item), '}'),
    _module_item: $ => choice($.field, $.requirement, $.verification, $.module, $.class, $.sequence, $.state),

    // ---- process artefacts ------------------------------------------------
    requirement: $ => seq('requirement', field('id', $.name), '{', repeat(choice($.field, $.annotation, $.verification)), '}'),
    verification: $ => seq('verification', field('id', $.name), '{', repeat(choice($.field, $.annotation)), '}'),

    // ---- black box --------------------------------------------------------
    boundary: $ => seq('boundary', field('name', $.name), '{', repeat(choice($.field, $.annotation, $.operation)), '}'),
    operation: $ => seq('operation', field('name', $.name), $.parameters, optional(seq('->', field('returns', $.type)))),

    // ---- white box: static ------------------------------------------------
    class: $ => seq('class', field('name', $.name), optional(seq(':', field('base', $.qualified_name))), '{', repeat(choice($.field, $.annotation, $.method, $.attribute)), '}'),
    method: $ => prec.right(seq(field('visibility', $.visibility), field('name', $.name), $.parameters, optional(seq('->', field('returns', $.type))), repeat($.contract))),
    attribute: $ => seq(field('visibility', $.visibility), field('name', $.name), ':', field('type', $.type)),
    visibility: $ => choice('+', '-', '#'),
    parameters: $ => seq('(', optional(seq($.parameter, repeat(seq(',', $.parameter)))), ')'),
    parameter: $ => seq(field('name', $.name), optional(seq(':', field('type', $.type)))),
    type: $ => $.qualified_name,
    contract: $ => seq('[', optional(seq(field('key', $.name), ':')), field('body', $.contract_body), ']'),
    contract_body: $ => /[^\]\n]+/,

    // ---- white box: dynamic -----------------------------------------------
    sequence: $ => seq('sequence', field('name', $.name), '{', repeat(choice($.field, $.annotation, $.participant, $.message, $.return)), '}'),
    participant: $ => seq('participant', field('name', $.name), ':', field('type', $.qualified_name)),
    message: $ => seq(field('from', $.name), '->', field('to', $.name), '.', field('op', $.name), $.arguments),
    return: $ => seq('return', field('to', $.name), '.', field('op', $.name), '->', field('value', $.name)),
    arguments: $ => seq('(', optional(seq($.name, repeat(seq(',', $.name)))), ')'),

    state: $ => seq('state', field('entity', $.qualified_name), '{', repeat(choice($.field, $.annotation, $.initial, $.transition)), '}'),
    initial: $ => seq('initial', field('name', $.name)),
    transition: $ => seq(field('from', $.name), '->', field('to', $.name), 'on', field('event', $.name)),

    // ---- representations --------------------------------------------------
    diagram: $ => seq('diagram', field('name', $.name), '{', repeat($.field), '}'),

    // ---- common -----------------------------------------------------------
    annotation: $ => seq('@', field('kind', $.name), field('target', $.qualified_name)),
    field: $ => seq(field('key', $.name), ':', optional(field('value', choice($.block_text, $.line_text)))),
    line_text: $ => /[^\n]+/,
    qualified_name: $ => seq($.name, repeat(seq('::', choice($.name, '*')))),
    name: $ => NAME,
  },
});
