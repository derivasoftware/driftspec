// The engine for a page in a browser, compiled to WebAssembly: one exported function over
// ds::api::Browser (REQ-00113-1). The page calls it once per edit and copies the answer out
// before the next call.

#include <string>

#include "ds/api/Browser.hpp"

extern "C" const char* ds_playground(const char* request) {
  static std::string answer;
  answer = ds::api::Browser::answer(request ? request : "{}");
  return answer.c_str();
}
