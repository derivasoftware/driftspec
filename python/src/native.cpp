// driftspec._native: the engine for Python, through the limited CPython API (abi3), so one
// wheel serves every Python from 3.10 on (DS-84). A Workspace is loaded once and kept here, in
// the engine, for every question the caller asks (DS-173); its answers are the JSON the engine
// writes, which the Python face reads into its own types. The functions of text to JSON stay,
// each a workspace loaded for one question.
#define Py_LIMITED_API 0x030A0000
#include <Python.h>

#include <exception>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

// the language, made once where the module starts and handed by const reference to every Workspace and
// to the engine's functions of text to JSON (DS-341); it lives as long as the module
const ds::model::Metamodel kMetamodel;
const ds::api::Api kApi(kMetamodel);

// a dict {path: text} as the engine's files; false, with a Python error set, when it is not one
bool filesOf(PyObject* dict, ds::api::Files* out) {
  if (!PyDict_Check(dict)) {
    PyErr_SetString(PyExc_TypeError, "files must be a dict of path -> text");
    return false;
  }
  PyObject *key = nullptr, *value = nullptr;
  Py_ssize_t pos = 0;
  while (PyDict_Next(dict, &pos, &key, &value)) {
    Py_ssize_t klen = 0, vlen = 0;
    const char* k = PyUnicode_AsUTF8AndSize(key, &klen);
    const char* v = k ? PyUnicode_AsUTF8AndSize(value, &vlen) : nullptr;
    if (!k || !v) return false;  // not str: the conversion set the error
    (*out)[std::string(k, klen)] = std::string(v, vlen);
  }
  return true;
}

// runs the engine with the GIL released, turning a C++ failure into a Python exception
template <typename F>
PyObject* run(F work) {
  std::string out, failure;
  Py_BEGIN_ALLOW_THREADS try { out = work(); } catch (const std::exception& e) {
    failure = e.what();
  } catch (...) {
    failure = "the engine failed";
  }
  Py_END_ALLOW_THREADS if (!failure.empty()) {
    PyErr_SetString(PyExc_RuntimeError, failure.c_str());
    return nullptr;
  }
  return PyUnicode_FromStringAndSize(out.data(), static_cast<Py_ssize_t>(out.size()));
}

PyObject* version(PyObject*, PyObject*) { return PyUnicode_FromString(kMetamodel.version().c_str()); }

PyObject* severities(PyObject*, PyObject*) {
  return run([] { return kApi.severities(); });
}

PyObject* check(PyObject*, PyObject* args) {
  PyObject* dict = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "O", &dict) || !filesOf(dict, &files)) return nullptr;
  return run([&files] { return kApi.check(files); });
}

PyObject* exportModel(PyObject*, PyObject* args) {
  PyObject* dict = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "O", &dict) || !filesOf(dict, &files)) return nullptr;
  return run([&files] { return kApi.exportModel(files); });
}

PyObject* expectedAt(PyObject*, PyObject* args) {
  const char* text = nullptr;
  int line = 0, column = 0;
  if (!PyArg_ParseTuple(args, "sii", &text, &line, &column)) return nullptr;
  const std::string source(text);
  return run([&source, line, column] { return ds::api::Api::expectedAt(source, line, column); });
}

PyObject* render(PyObject*, PyObject* args) {
  PyObject* dict = nullptr;
  const char *diagram = nullptr, *format = nullptr;
  int width = 0;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "Ossi", &dict, &diagram, &format, &width) || !filesOf(dict, &files)) return nullptr;
  const std::string d(diagram), f(format);
  return run([&files, d, f, width] { return kApi.render(files, d, f, width); });
}

PyObject* page(PyObject*, PyObject* args) {
  PyObject *dict = nullptr, *ids = nullptr;
  const char* title = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "OsO", &dict, &title, &ids) || !filesOf(dict, &files)) return nullptr;
  std::vector<std::string> focus;
  PyObject* it = PyObject_GetIter(ids);
  if (!it) return nullptr;
  for (PyObject* item = PyIter_Next(it); item; item = PyIter_Next(it)) {
    Py_ssize_t n = 0;
    const char* s = PyUnicode_AsUTF8AndSize(item, &n);
    if (s) focus.emplace_back(s, n);
    Py_DECREF(item);
    if (!s) {
      Py_DECREF(it);
      return nullptr;
    }
  }
  Py_DECREF(it);
  const std::string t(title);
  return run([&files, t, &focus] { return kApi.page(files, t, focus); });
}

// ---- a workspace loaded once (DS-173) ------------------------------------------------------------

struct WorkspaceObject {
  PyObject_HEAD ds::api::Workspace* workspace;
};

PyTypeObject* workspaceType = nullptr;

// the strings a Python iterable holds; false, with a Python error set, when one is not a str
bool stringsOf(PyObject* iterable, std::vector<std::string>* out) {
  PyObject* it = PyObject_GetIter(iterable);
  if (!it) return false;
  for (PyObject* item = PyIter_Next(it); item; item = PyIter_Next(it)) {
    Py_ssize_t n = 0;
    const char* s = PyUnicode_AsUTF8AndSize(item, &n);
    if (s) out->emplace_back(s, n);
    Py_DECREF(item);
    if (!s) {
      Py_DECREF(it);
      return false;
    }
  }
  Py_DECREF(it);
  return !PyErr_Occurred();
}

PyObject* workspaceNew(PyTypeObject* type, PyObject* args, PyObject*) {
  PyObject* dict = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "O", &dict) || !filesOf(dict, &files)) return nullptr;
  ds::api::Workspace* loaded = nullptr;
  std::string failure;
  Py_BEGIN_ALLOW_THREADS try { loaded = new ds::api::Workspace(kMetamodel, files); } catch (const std::exception& e) {
    failure = e.what();
  }
  Py_END_ALLOW_THREADS if (!loaded) {
    PyErr_SetString(PyExc_RuntimeError, failure.empty() ? "the engine failed" : failure.c_str());
    return nullptr;
  }
  auto alloc = reinterpret_cast<allocfunc>(PyType_GetSlot(type, Py_tp_alloc));
  auto* self = reinterpret_cast<WorkspaceObject*>(alloc(type, 0));
  if (!self) {
    delete loaded;
    return nullptr;
  }
  self->workspace = loaded;
  return reinterpret_cast<PyObject*>(self);
}

void workspaceDealloc(PyObject* self) {
  delete reinterpret_cast<WorkspaceObject*>(self)->workspace;
  PyTypeObject* type = Py_TYPE(self);
  auto release = reinterpret_cast<freefunc>(PyType_GetSlot(type, Py_tp_free));
  release(self);
  Py_DECREF(type);
}

const ds::api::Workspace& held(PyObject* self) { return *reinterpret_cast<WorkspaceObject*>(self)->workspace; }

// a question of no argument, of one string, of two strings, or of two strings and an int
template <std::string (ds::api::Workspace::*Q)() const>
PyObject* ask0(PyObject* self, PyObject*) {
  return run([self] { return (held(self).*Q)(); });
}

template <std::string (ds::api::Workspace::*Q)(const std::string&) const>
PyObject* ask1(PyObject* self, PyObject* args) {
  const char* a = nullptr;
  if (!PyArg_ParseTuple(args, "s", &a)) return nullptr;
  const std::string x(a);
  return run([self, x] { return (held(self).*Q)(x); });
}

template <std::string (ds::api::Workspace::*Q)(const std::string&, const std::string&) const>
PyObject* ask2(PyObject* self, PyObject* args) {
  const char *a = nullptr, *b = nullptr;
  if (!PyArg_ParseTuple(args, "ss", &a, &b)) return nullptr;
  const std::string x(a), y(b);
  return run([self, x, y] { return (held(self).*Q)(x, y); });
}

template <std::string (ds::api::Workspace::*Q)(const std::string&, const std::string&, int) const>
PyObject* ask3(PyObject* self, PyObject* args) {
  const char *a = nullptr, *b = nullptr;
  int n = 0;
  if (!PyArg_ParseTuple(args, "ssi", &a, &b, &n)) return nullptr;
  const std::string x(a), y(b);
  return run([self, x, y, n] { return (held(self).*Q)(x, y, n); });
}

PyObject* workspacePage(PyObject* self, PyObject* args) {
  const char* title = nullptr;
  PyObject* ids = nullptr;
  std::vector<std::string> focus;
  if (!PyArg_ParseTuple(args, "sO", &title, &ids) || !stringsOf(ids, &focus)) return nullptr;
  const std::string t(title);
  return run([self, t, &focus] { return held(self).page(t, focus); });
}

PyMethodDef kWorkspaceMethods[] = {
    {"export", ask0<&ds::api::Workspace::exportModel>, METH_NOARGS, "export() -> str: the model, as ds export"},
    {"report", ask0<&ds::api::Workspace::report>, METH_NOARGS, "report() -> str: the findings and counts, as ds check --json"},
    {"find", ask1<&ds::api::Workspace::find>, METH_VARARGS, "find(id) -> str: one entity, or null"},
    {"relations_from", ask1<&ds::api::Workspace::from>, METH_VARARGS, "relations_from(id) -> str: what leaves it, in the order stated"},
    {"relations_to", ask1<&ds::api::Workspace::to>, METH_VARARGS, "relations_to(id) -> str: what reaches it, in the order stated"},
    {"touching", ask1<&ds::api::Workspace::touching>, METH_VARARGS, "touching(id) -> str: what leaves or reaches it, each once"},
    {"members", ask1<&ds::api::Workspace::members>, METH_VARARGS, "members(scope) -> str: the identities it declares"},
    {"parent", ask1<&ds::api::Workspace::parent>, METH_VARARGS, "parent(id) -> str: what holds it"},
    {"children", ask1<&ds::api::Workspace::children>, METH_VARARGS, "children(id) -> str: what it holds, in order"},
    {"revision_of", ask1<&ds::api::Workspace::revisionOf>, METH_VARARGS, "revision_of(artefact) -> str: the declared revision"},
    {"tree", ask1<&ds::api::Workspace::tree>, METH_VARARGS, "tree(sequence) -> str: its messages and frames, as written"},
    {"steps", ask2<&ds::api::Workspace::steps>, METH_VARARGS, "steps(sequence, path) -> str: what a scenario's path takes"},
    {"walk", ask3<&ds::api::Workspace::walk>, METH_VARARGS, "walk(sequence, path, depth) -> str: into what it calls"},
    {"ask", ask2<&ds::api::Workspace::ask>, METH_VARARGS, "ask(question, subject) -> str: as ds query --json"},
    {"shows", ask1<&ds::api::Workspace::shows>, METH_VARARGS, "shows(diagram) -> str: what a diagram takes"},
    {"render", ask3<&ds::api::Workspace::render>, METH_VARARGS, "render(diagram, format, width) -> str: '' when none"},
    {"page", workspacePage, METH_VARARGS, "page(title, focus) -> str: the viewer, framed on the identities given"},
    {nullptr, nullptr, 0, nullptr}};

PyType_Slot kWorkspaceSlots[] = {
    {Py_tp_new, reinterpret_cast<void*>(workspaceNew)},
    {Py_tp_dealloc, reinterpret_cast<void*>(workspaceDealloc)},
    {Py_tp_methods, kWorkspaceMethods},
    {Py_tp_doc, const_cast<char*>("Workspace(files: dict[str, str]): a workspace loaded once, asked as often as needed")},
    {0, nullptr}};

PyType_Spec kWorkspaceSpec = {"driftspec._native.Workspace", sizeof(WorkspaceObject), 0, Py_TPFLAGS_DEFAULT, kWorkspaceSlots};

// ---- how an identity is written (DS-173) ---------------------------------------------------------

template <std::string (*F)(const std::string&)>
PyObject* identity1(PyObject*, PyObject* args) {
  const char* a = nullptr;
  if (!PyArg_ParseTuple(args, "s", &a)) return nullptr;
  const std::string x(a);
  return run([x] { return F(x); });
}

PyObject* qualify(PyObject*, PyObject* args) {
  const char *scope = nullptr, *name = nullptr;
  if (!PyArg_ParseTuple(args, "ss", &scope, &name)) return nullptr;
  const std::string s(scope), n(name);
  return run([s, n] { return ds::api::Identity::qualify(s, n); });
}

PyMethodDef kMethods[] = {
    {"qualify", qualify, METH_VARARGS, "qualify(scope, name) -> str: a name written in a scope, as an identity"},
    {"outer", identity1<&ds::api::Identity::outer>, METH_VARARGS, "outer(scope) -> str: the scope around it"},
    {"box_of", identity1<&ds::api::Identity::boxOf>, METH_VARARGS, "box_of(scope) -> str: the box a scope names"},
    {"package_of", identity1<&ds::api::Identity::packageOf>, METH_VARARGS, "package_of(scope) -> str: its innermost package scope"},
    {"behaviour_of", identity1<&ds::api::Identity::behaviourOf>, METH_VARARGS, "behaviour_of(id) -> str: the behaviour it is or lies in"},
    {"subject_of", identity1<&ds::api::Identity::subjectOf>, METH_VARARGS, "subject_of(id) -> str: the subject of a behaviour"},
    {"scenario_of", identity1<&ds::api::Identity::scenarioOf>, METH_VARARGS, "scenario_of(id) -> str: the scenario a behaviour names"},
    {"without_revision", identity1<&ds::api::Identity::withoutRevision>, METH_VARARGS, "without_revision(id) -> str: an artefact id bare"},
    {"version", version, METH_NOARGS, "version() -> str: the version of the engine, as ds --version says it"},
    {"severities", severities, METH_NOARGS, "severities() -> str: the severity table, as JSON (DS-386)"},
    {"check", check, METH_VARARGS, "check(files: dict[str, str]) -> str: the findings and counts, as ds check --json"},
    {"export", exportModel, METH_VARARGS, "export(files: dict[str, str]) -> str: the model, as ds export"},
    {"expected_at", expectedAt, METH_VARARGS, "expected_at(text, line, column) -> str: what the grammar takes there, as JSON"},
    {"render", render, METH_VARARGS, "render(files, diagram, format, width) -> str: a diagram drawn as text or svg; '' when none"},
    {"page", page, METH_VARARGS, "page(files, title, focus) -> str: the viewer, framed on the identities given"},
    {nullptr, nullptr, 0, nullptr}};

PyModuleDef kModule = {
    PyModuleDef_HEAD_INIT, "_native", "The DriftSpec engine, in process.", -1, kMethods, nullptr, nullptr, nullptr, nullptr};

}  // namespace

PyMODINIT_FUNC PyInit__native(void) {
  PyObject* module = PyModule_Create(&kModule);
  if (!module) return nullptr;
  workspaceType = reinterpret_cast<PyTypeObject*>(PyType_FromSpec(&kWorkspaceSpec));
  if (!workspaceType || PyModule_AddObject(module, "Workspace", reinterpret_cast<PyObject*>(workspaceType)) < 0) {
    Py_XDECREF(workspaceType);
    Py_DECREF(module);
    return nullptr;
  }
  Py_INCREF(workspaceType);  // the module holds one reference, this file another
  return module;
}
