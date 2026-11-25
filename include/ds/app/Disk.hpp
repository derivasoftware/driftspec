#ifndef DRIFTSPEC_APP_DISK_HPP
#define DRIFTSPEC_APP_DISK_HPP

#include <string>

#include "ds/api/Workspace.hpp"

namespace ds {
namespace app {

/// What the command line reads from the disk (DS-83, DS-409): whether a path is a workspace, a workspace's files as
/// the Api takes them, a file read whole (a stylesheet, a render-IR document) and the title a workspace's page has.
class Disk {
 public:
  static bool isDirectory(const std::string& path);
  /// Whether any .ds file lies under a directory: a directory with none is no workspace (a mistyped path, most often).
  static bool holdsDesign(const std::string& dir);
  /// Why `dir` is no workspace, as ds says it: not a directory, or one with no .ds file under it; "" when it is one.
  static std::string whyNoWorkspace(const std::string& dir);
  /// Every .ds file under `dir`, by its path, read whole as it is on the disk, in the order of their paths; a file that
  /// cannot be read fails, std::runtime_error "cannot read <path>".
  static api::Files files(const std::string& dir);
  /// Whether a file can be opened to be read.
  static bool readable(const std::string& path);
  /// A file read whole; "" when it cannot be read.
  static std::string read(const std::string& path);
  /// What a workspace's page is titled after: the last name of its path, the slashes after it left out.
  static std::string titleOf(std::string dir);
};

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_DISK_HPP
