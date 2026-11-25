#include "ds/app/Disk.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ds {
namespace app {

bool Disk::isDirectory(const std::string& path) {
  std::error_code error;
  return std::filesystem::is_directory(path, error);
}

bool Disk::holdsDesign(const std::string& dir) {
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
    if (it->is_regular_file() && it->path().extension() == ".ds") return true;
  return false;
}

std::string Disk::whyNoWorkspace(const std::string& dir) {
  // every command then ends in the door's outcome, exit 2: the command line names no design (DS-83)
  if (!isDirectory(dir)) return "cannot read " + dir + ": not a directory";
  if (!holdsDesign(dir)) return "no .ds file under " + dir + ": it is no workspace";
  return "";
}

// read as the parser has read a directory: every .ds file under it, in the order of their paths, each one whole as
// bytes; what the directory or a file refuses fails, and the command line says it, exit 2 (DS-279)
api::Files Disk::files(const std::string& dir) {
  std::vector<std::string> paths;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    if (entry.is_regular_file() && entry.path().extension() == ".ds") paths.push_back(entry.path().string());
  std::sort(paths.begin(), paths.end());
  api::Files files;
  for (const auto& path : paths) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    files.emplace(path, buffer.str());
  }
  return files;
}

bool Disk::readable(const std::string& path) { return static_cast<bool>(std::ifstream(path)); }

std::string Disk::read(const std::string& path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::string Disk::titleOf(std::string dir) {
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const auto slash = dir.find_last_of('/');
  return slash == std::string::npos ? dir : dir.substr(slash + 1);
}

}  // namespace app
}  // namespace ds
