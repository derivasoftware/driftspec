#ifndef DRIFTSPEC_APP_APP_HPP
#define DRIFTSPEC_APP_APP_HPP

// The ds command line as functions: the program's main (app/ds/main.cpp) hands its command line to
// ds::app::main, which runs it on a thread with a large stack (DS-279).

#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace app {

/// The command line run on a thread with a large stack: the exit code of the command (DS-279).
int main(int argc, char** argv);
/// The command named on the command line, run: its exit code.
int run(int argc, char** argv);
/// The command run: what cannot be read is said, with exit 2, never an abort (DS-279).
int work(int argc, char** argv);

bool isDirectory(const std::string& path);
ds::model::Model load(const ds::model::Metamodel& metamodel, const std::string& dir);
/// Whether any .ds file lies under a directory: a directory with none is no workspace (a mistyped path, most often).
bool holdsDesign(const std::string& dir);
/// The workspace loaded, or nothing when the path is no workspace (reported on stderr) (DS-83).
bool loadWorkspace(const ds::model::Metamodel& metamodel, const std::string& dir, ds::model::Model* model);
std::string titleOf(std::string dir);
std::string readFile(const std::string& path);
/// Writes to the file -o names, or to stdout.
int emit(const std::string& output, const std::string& text);
/// A page of the manual ds embeds; an unknown topic lists the ones there are.
int manual(const ds::model::Metamodel& metamodel, const std::string& topic, bool json);
int check(const ds::model::Metamodel& metamodel, const std::string& dir, bool json);
std::string diagramId(const std::string& name);

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_APP_HPP
