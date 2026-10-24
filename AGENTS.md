# Working in this repository

This project is **NA**.

## Orient yourself — run this first

```bash
argos context              # artefact counts, next IDs, conventions, active diagnostics
argos tree                 # requirement DAG with status and ASIL
```

## Workflow rules

- **Every task starts with PROC-00014-2 (Start Task).** Read it before writing
  any artefact:
  ```bash
  argos get PROC-00014-2
  ```
- This project is **NA** (no ASIL declared). Agents may act as Owner
  under PROTOCOL-autonomous-loop (Autonomous Development Loop). The loop
  reads this signal from ``argos.toml [project].asil``.

- New artefacts start at DRAFT. Promote to APPROVED only after Owner sign-off.
- Commits: imperative summary of what was produced or changed.
- Work reaches `main` through a merge request, never a direct push — a merge
  request never merges itself. `.pre-commit-config.yaml` carries the
  `no-push-to-main` guard; install it once per clone, hooks included:
  ```bash
  pre-commit install --hook-type pre-commit --hook-type pre-push
  ```

## Test layout

C++ tests live under ``tests/unit/<package>/`` where ``<package>``
matches the namespace declared in the source tree under ``src/``
(e.g. ``namespace geom { class Circle … }`` in ``src/geom/circle.hpp``
→ ``tests/unit/geom/test_circle.cpp``). Test files outside that tree
fire ``W-TEST-LAYOUT-UNRECOGNISED`` and stop participating in
class→test traceability (``W-CLASS-NO-TEST``). The fixture-class
convention is ``Test{X}``; typed/parametric fixtures may carry a
trailing ``Typed``, ``TypedTests``, ``P``, ``ParamTest`` or
``Parametrised`` marker that the heuristic peels back to ``X``.

## Design layout

CL_ diagrams mirror the namespace they document (``CST-00013-1``):
a class in ``namespace geom::shapes`` belongs in
``design/lld/geom/shapes/CL_<Class>.puml``. Authoring a CL_ at
``design/lld/CL_<Class>.puml`` (flat, no namespace subdir) fires
``W-DESIGN-WRONG-PATH``. Use ``argos new lld-aggregate <module>`` to
scaffold both the namespace directory and a per-module rollup
aggregate (``CL_<Module>.puml``) that ``!includesub`` s the leaf CL_
files — that aggregate is what closes ``W-LLD-AGGREGATE-LEAF-ORPHAN``
and ``W-LLD-AGGREGATE-INCOMPLETE`` once the HLD references the
module.

## Definition of Done

```bash
cmake --build build && ctest --test-dir build && pre-commit run --all-files
```
