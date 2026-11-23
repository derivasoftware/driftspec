// The engine for a page in a browser, compiled to WebAssembly: one exported function over
// ds::api::Browser (REQ-00113-1). The page calls it once per edit and copies the answer out
// before the next call. The metamodel is made here, where the engine starts, on the first call,
// and handed to the browser's engine by const reference (DS-341).

#include <string>

#include "ds/api/Browser.hpp"
#include "ds/model/Metamodel.hpp"

extern "C" const char* ds_playground(const char* request) {
  static const ds::model::Metamodel metamodel;
  static const ds::api::Browser browser(metamodel);
  static std::string answer;
  answer = browser.answer(request ? request : "{}");
  return answer.c_str();
}
