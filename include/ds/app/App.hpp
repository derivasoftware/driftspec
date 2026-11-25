#ifndef DRIFTSPEC_APP_APP_HPP
#define DRIFTSPEC_APP_APP_HPP

// The ds command line's entry (DS-311, DS-409): the program's main (app/ds/main.cpp) hands its command line to
// ds::app::main, which runs it on a thread with a large stack (DS-279). main is the one function of app, since C++
// starts a program at one; everything else it does is a member of a class: CommandLine reads the command line and
// runs the verb named, each verb an ICommand that CommandFactory makes.

namespace ds {
namespace app {

/// The command line run by a CommandLine on a thread with a large stack: the exit code of the command (DS-279).
int main(int argc, char** argv);

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_APP_HPP
