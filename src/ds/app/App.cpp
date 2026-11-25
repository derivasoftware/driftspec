// ds — the DriftSpec command line: check, export, render, query and the manual over a workspace.
#include "ds/app/App.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>

#include <cstdio>
#else
#include <pthread.h>
#endif

#include <cstddef>
#include <string>
#include <vector>

#include "ds/app/CommandLine.hpp"

namespace ds {
namespace app {

// The work runs on a thread with a large stack: the binder walks a design as deep as it is nested, and the
// 8 MB of the main thread end near 1,500 nested blocks (DS-279). The memory is reserved, not used. On Windows
// the stack of the main thread is set when ds is linked (DS-283), and the work runs there.
int main(int argc, char** argv) {
#ifdef _WIN32
  // what ds writes is the same bytes on every system: no line ending is turned into \r\n (DS-283)
  _setmode(_fileno(stdout), _O_BINARY);
  return CommandLine().run(std::vector<std::string>(argv, argv + argc));
#else
  struct Call {  // what the thread that does the work is given and gives back
    int argc;
    char** argv;
    int code;
  } call{argc, argv, 2};
  const auto thread = [](void* raw) -> void* {
    auto* c = static_cast<Call*>(raw);
    c->code = CommandLine().run(std::vector<std::string>(c->argv, c->argv + c->argc));
    return nullptr;
  };
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, std::size_t{512} * 1024 * 1024);
  pthread_t worker;
  if (pthread_create(&worker, &attributes, thread, &call) != 0) {
    // no thread to be had: the work runs here, on the stack there is
    call.code = CommandLine().run(std::vector<std::string>(argv, argv + argc));
  } else {
    pthread_join(worker, nullptr);
  }
  pthread_attr_destroy(&attributes);
  return call.code;
#endif
}

}  // namespace app
}  // namespace ds
