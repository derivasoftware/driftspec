// driftspec._native: the engine for Python, through the limited CPython API (abi3), so one
// wheel serves every Python from 3.10 on (DS-84). Text in, the JSON of the command line out;
// the Python face builds its model from that JSON.
#define Py_LIMITED_API 0x030A0000
#include <Python.h>

#include <exception>
#include <string>

#include "ds/api/Api.hpp"

namespace {

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

PyObject* check(PyObject*, PyObject* args) {
  PyObject* dict = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "O", &dict) || !filesOf(dict, &files)) return nullptr;
  return run([&files] { return ds::api::Api::check(files); });
}

PyObject* exportModel(PyObject*, PyObject* args) {
  PyObject* dict = nullptr;
  ds::api::Files files;
  if (!PyArg_ParseTuple(args, "O", &dict) || !filesOf(dict, &files)) return nullptr;
  return run([&files] { return ds::api::Api::exportModel(files); });
}

PyObject* expectedAt(PyObject*, PyObject* args) {
  const char* text = nullptr;
  int line = 0, column = 0;
  if (!PyArg_ParseTuple(args, "sii", &text, &line, &column)) return nullptr;
  const std::string source(text);
  return run([&source, line, column] { return ds::api::Api::expectedAt(source, line, column); });
}

PyMethodDef kMethods[] = {
    {"check", check, METH_VARARGS, "check(files: dict[str, str]) -> str: the findings and counts, as ds check --json"},
    {"export", exportModel, METH_VARARGS, "export(files: dict[str, str]) -> str: the model, as ds export"},
    {"expected_at", expectedAt, METH_VARARGS, "expected_at(text, line, column) -> str: what the grammar takes there, as JSON"},
    {nullptr, nullptr, 0, nullptr}};

PyModuleDef kModule = {
    PyModuleDef_HEAD_INIT, "_native", "The DriftSpec engine, in process.", -1, kMethods, nullptr, nullptr, nullptr, nullptr};

}  // namespace

PyMODINIT_FUNC PyInit__native(void) { return PyModule_Create(&kModule); }
