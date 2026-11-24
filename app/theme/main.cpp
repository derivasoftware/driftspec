// ds_theme: what one writer of the theme writes (DS-393). Built from the theme's sources alone and never shipped; the
// pages carry what it writes between their theme markers, and tools/theme.py checks or rewrites them with it.
//   ds_theme <form>     svg, sequence, viewer, viewer-night, live, playground, design-page
#include <iostream>
#include <string>

#include "ds/theme/Theme.hpp"
#include "ds/theme/Writers.hpp"

int main(int argc, char** argv) {
  const ds::theme::Theme theme;
  const auto writers = ds::theme::ThemeWriterFactory::of();
  if (argc != 2 || !writers.count(argv[1])) {
    std::cerr << "usage: ds_theme <form>; the forms:";
    for (const auto& w : writers) std::cerr << " " << w.first;
    std::cerr << "\n";
    return 2;
  }
  std::cout << writers.at(argv[1])->write(theme);
  return 0;
}
