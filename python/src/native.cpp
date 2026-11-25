// driftspec._native: the engine for Python, through the limited CPython API (abi3), so one
// wheel serves every Python from 3.10 on (DS-84). A Workspace is loaded once and kept here, in
// the engine, for every question the caller asks (DS-173); its answers are the JSON the engine
// writes, which the Python face reads into its own types. The functions of text to JSON stay,
// each a workspace loaded for one question.
#define Py_LIMITED_API 0x030A0000
#include <Python.h>

#include <cstdint>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/service/UseCase.hpp"

namespace {

// the language, made once where the module starts and handed by const reference to every Workspace and
// to the engine's functions of text to JSON (DS-341); it lives as long as the module
const ds::model::Metamodel kMetamodel;
const ds::api::Api kApi(kMetamodel);

// a dict {path: text} as the engine's files, each text a str or the file's bytes as they are, so a file that is not
// UTF-8 reaches the engine and is its finding, as on the command line (DS-411); false, with a Python error set, when
// it is not one
bool filesOf(PyObject* dict, ds::api::Files* out) {
  if (!PyDict_Check(dict)) {
    PyErr_SetString(PyExc_TypeError, "files must be a dict of path -> text or bytes");
    return false;
  }
  PyObject *key = nullptr, *value = nullptr;
  Py_ssize_t pos = 0;
  while (PyDict_Next(dict, &pos, &key, &value)) {
    Py_ssize_t klen = 0, vlen = 0;
    const char* k = PyUnicode_AsUTF8AndSize(key, &klen);
    if (!k) return false;  // not str: the conversion set the error
    const char* v = nullptr;
    if (PyBytes_Check(value)) {
      char* bytes = nullptr;
      if (PyBytes_AsStringAndSize(value, &bytes, &vlen) < 0) return false;
      v = bytes;
    } else if (!(v = PyUnicode_AsUTF8AndSize(value, &vlen))) {
      return false;  // neither bytes nor str: the conversion set the error
    }
    (*out)[std::string(k, klen)] = std::string(v, vlen);
  }
  return true;
}

// runs the engine with the GIL released, turning a C++ failure into a Python exception: a pair the engine does not
// write (emit::NotImplemented) into NotImplementedError(message, document, technology), which the package raises as
// its own driftspec.NotImplemented (DS-398); any other into RuntimeError
template <typename F>
PyObject* run(F work) {
  std::string out, failure, document, technology;
  bool notImplemented = false;
  Py_BEGIN_ALLOW_THREADS try { out = work(); } catch (const ds::emit::NotImplemented& e) {
    failure = e.what();
    document = e.document();
    technology = e.technology();
    notImplemented = true;
  } catch (const std::exception& e) {
    failure = e.what();
  } catch (...) {
    failure = "the engine failed";
  }
  Py_END_ALLOW_THREADS if (notImplemented) {
    PyObject* args = Py_BuildValue("(sss)", failure.c_str(), document.c_str(), technology.c_str());
    if (args) {
      PyErr_SetObject(PyExc_NotImplementedError, args);
      Py_DECREF(args);
    }
    return nullptr;
  }
  if (!failure.empty()) {
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
  Py_BEGIN_ALLOW_THREADS try {
    loaded = new ds::api::Workspace(ds::api::Workspace::madeFor(kMetamodel, files));
  } catch (const std::exception& e) {
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

// ---- a use case described as Python objects (DS-459) ----------------------------------------------

// The interim sink of the driftdomain seam (DS-44): it builds the Python objects a description writes, a record a
// dict (its fields in the order written, the order json.loads gave them), a list a list, a text a str, an integer an
// int, a number a float (None when it is not finite, as the JSON sink writes null), a boolean a bool and no value
// None. Nothing is written as text to be parsed back. It is used with the GIL held.
class PySink : public ds::emit::ISink {
 public:
  ~PySink() override { Py_XDECREF(root_); }
  ds::emit::ISink& beginRecord() override { return open(PyDict_New()); }
  ds::emit::ISink& endRecord() override { return close(); }
  ds::emit::ISink& field(std::string_view name) override {
    key_ = std::string(name);
    return *this;
  }
  ds::emit::ISink& beginList() override { return open(PyList_New(0)); }
  ds::emit::ISink& endList() override { return close(); }
  ds::emit::ISink& value(std::string_view text) override {
    return put(PyUnicode_DecodeUTF8(text.data(), static_cast<Py_ssize_t>(text.size()), "strict"));
  }
  ds::emit::ISink& integer(std::int64_t value) override { return put(PyLong_FromLongLong(value)); }
  ds::emit::ISink& number(double value) override {
    if (value != value || value - value != 0) return null();  // not finite: null, as the JSON sink writes it
    return put(PyFloat_FromDouble(value));
  }
  ds::emit::ISink& boolean(bool value) override { return put(PyBool_FromLong(value ? 1 : 0)); }
  ds::emit::ISink& null() override {
    Py_INCREF(Py_None);
    return put(Py_None);
  }
  // the presentation (DS-399) is the text sink's: a document of records, lists and values takes none of it, as the
  // JSON sink takes none (DS-475)
  ds::emit::ISink& paragraph(std::string_view) override { return *this; }
  ds::emit::ISink& table(const std::vector<std::string_view>&) override { return *this; }
  ds::emit::ISink& row(const std::vector<std::string_view>&) override { return *this; }
  ds::emit::ISink& endTable() override { return *this; }
  /// A text already written in its technology, a drawing or the viewer's page (DS-474): kept as the str it is, the
  /// object built, so the Python face gets the drawing as ds render writes it. Once per document: a second one is
  /// refused.
  ds::emit::ISink& verbatim(std::string_view text) override {
    if (verbatim_) throw std::logic_error("PySink: a second verbatim text in one document");
    verbatim_ = true;
    return put(PyUnicode_DecodeUTF8(text.data(), static_cast<Py_ssize_t>(text.size()), "strict"));
  }
  /// The description of `document` ends: one that built nothing here fails NotImplemented, as the JSON sink does.
  ds::emit::ISink& described(std::string_view document) override {
    if (!root_) throw ds::emit::NotImplemented(std::string(document), "python", {});
    return *this;
  }
  /// A text is no document of this sink: what it built is taken with take().
  std::string result() override { return ""; }
  /// The object built, handed over (a new reference); None when nothing was written.
  PyObject* take() {
    PyObject* out = root_ ? root_ : (Py_INCREF(Py_None), Py_None);
    root_ = nullptr;
    open_.clear();
    verbatim_ = false;
    return out;
  }

 private:
  // a value where the sink stands: the document, an element of the open list or the field named of the open record
  ds::emit::ISink& put(PyObject* value) {
    if (!value) throw std::runtime_error("a Python object could not be made");
    if (open_.empty()) {
      Py_XDECREF(root_);
      root_ = value;
      return *this;
    }
    PyObject* at = open_.back();
    const int failed = PyList_Check(at) ? PyList_Append(at, value) : PyDict_SetItemString(at, key_.c_str(), value);
    Py_DECREF(value);
    if (failed) throw std::runtime_error("a Python object could not be made");
    return *this;
  }
  ds::emit::ISink& open(PyObject* container) {
    if (container) Py_INCREF(container);  // held while open; the place it is put in holds it after
    put(container);
    open_.push_back(container);
    Py_DECREF(container);
    return *this;
  }
  ds::emit::ISink& close() {
    open_.pop_back();
    return *this;
  }

  PyObject* root_ = nullptr;
  std::vector<PyObject*> open_;  ///< the record or list being written, innermost last (borrowed: their parent holds them)
  std::string key_;
  bool verbatim_ = false;  ///< a verbatim text is the object built since the last take
};

// a request as a use case reads it: its arguments by name, each a text
bool requestOf(PyObject* dict, ds::service::Request* out) {
  if (!PyDict_Check(dict)) {
    PyErr_SetString(PyExc_TypeError, "a request is a dict of name -> text");
    return false;
  }
  PyObject *key = nullptr, *value = nullptr;
  Py_ssize_t at = 0;
  while (PyDict_Next(dict, &at, &key, &value)) {
    Py_ssize_t klen = 0, vlen = 0;
    const char* k = PyUnicode_AsUTF8AndSize(key, &klen);
    const char* v = k ? PyUnicode_AsUTF8AndSize(value, &vlen) : nullptr;
    if (!v) return false;
    out->arguments[std::string(k, klen)] = std::string(v, vlen);
  }
  return true;
}

// run(name, request) -> (outcome, object, message): the use case of that name over the design the workspace holds
// (DS-459), and what its outcome says, "" when it says nothing (DS-479).
// The model is loaded with the GIL released, then the use case describes its document as Python objects, holding it;
// a request that is not the use case's is ValueError, what the sink cannot write NotImplementedError as elsewhere
PyObject* workspaceRun(PyObject* self, PyObject* args) {
  const char* name = nullptr;
  PyObject* dict = nullptr;
  ds::service::Request request;
  if (!PyArg_ParseTuple(args, "sO", &name, &dict) || !requestOf(dict, &request)) return nullptr;
  const std::string use(name);
  ds::api::Workspace* workspace = reinterpret_cast<WorkspaceObject*>(self)->workspace;
  std::string failure, document, technology;
  enum class Failed { none, request, notImplemented, other } failed = Failed::none;
  const auto said = [&](const std::exception& e, Failed kind) {
    failure = e.what();
    failed = kind;
  };
  Py_BEGIN_ALLOW_THREADS try { workspace->prepare(use, request); } catch (const ds::service::BadRequest& e) {
    said(e, Failed::request);
  } catch (const ds::emit::NotImplemented& e) {
    said(e, Failed::notImplemented);
    document = e.document();
    technology = e.technology();
  } catch (const std::exception& e) {
    said(e, Failed::other);
  }
  Py_END_ALLOW_THREADS PySink sink;
  ds::service::Outcome outcome;
  if (failed == Failed::none) {
    try {
      outcome = workspace->run(use, request, sink);
    } catch (const ds::service::BadRequest& e) {
      said(e, Failed::request);
    } catch (const ds::emit::NotImplemented& e) {
      said(e, Failed::notImplemented);
      document = e.document();
      technology = e.technology();
    } catch (const std::exception& e) {
      said(e, Failed::other);
    }
  }
  if (failed == Failed::request) {
    PyErr_SetString(PyExc_ValueError, failure.c_str());
    return nullptr;
  }
  if (failed == Failed::notImplemented) {
    PyObject* raised = Py_BuildValue("(sss)", failure.c_str(), document.c_str(), technology.c_str());
    if (raised) {
      PyErr_SetObject(PyExc_NotImplementedError, raised);
      Py_DECREF(raised);
    }
    return nullptr;
  }
  if (failed == Failed::other) {
    if (!PyErr_Occurred()) PyErr_SetString(PyExc_RuntimeError, failure.c_str());
    return nullptr;
  }
  PyObject* built = sink.take();
  PyObject* out = Py_BuildValue("(sNs)", outcome.name.c_str(), built, outcome.message.c_str());
  return out;
}

// reload(files): the workspace loaded again from the files given, in place (DS-420)
PyObject* workspaceReload(PyObject* self, PyObject* args) {
  PyObject* dict = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "O", &dict) || !filesOf(dict, &files)) return nullptr;
  std::string failure;
  ds::api::Workspace* workspace = reinterpret_cast<WorkspaceObject*>(self)->workspace;
  Py_BEGIN_ALLOW_THREADS try { workspace->reload(files); } catch (const std::exception& e) {
    failure = e.what();
  } catch (...) {
    failure = "the engine failed";
  }
  Py_END_ALLOW_THREADS if (!failure.empty()) {
    PyErr_SetString(PyExc_RuntimeError, failure.c_str());
    return nullptr;
  }
  Py_RETURN_NONE;
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
    {"reload", workspaceReload, METH_VARARGS, "reload(files) -> None: loaded again from the files given, in place"},
    {"run", workspaceRun, METH_VARARGS,
     "run(name, request) -> (outcome, object, message): a use case, described as Python objects, and what its outcome says"},
    {nullptr, nullptr, 0, nullptr}};

PyType_Slot kWorkspaceSlots[] = {
    {Py_tp_new, reinterpret_cast<void*>(workspaceNew)},
    {Py_tp_dealloc, reinterpret_cast<void*>(workspaceDealloc)},
    {Py_tp_methods, kWorkspaceMethods},
    {Py_tp_doc, const_cast<char*>("Workspace(files: dict[str, str | bytes]): a workspace loaded once, asked as often as needed")},
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
    {"check", check, METH_VARARGS, "check(files: dict[str, str | bytes]) -> str: the findings and counts, as ds check --json"},
    {"export", exportModel, METH_VARARGS, "export(files: dict[str, str | bytes]) -> str: the model, as ds export"},
    {"expected_at", expectedAt, METH_VARARGS, "expected_at(text, line, column) -> str: what the grammar takes there, as JSON"},
    {"render", render, METH_VARARGS, "render(files, diagram, format, width) -> str: drawn in svg, text, cells, layout or ir; '' when none"},
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
