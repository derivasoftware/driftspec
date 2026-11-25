The ds command line is a thin adapter over the use cases (DS-409), and nothing it prints or exits with changes.
`ds::app::CommandLine` owns the one CLI11 application, so the parse, the help, `--version` and the footers stay as
they were. Each verb is an `ICommand` (`CheckCommand`, `ExportCommand`, `RenderCommand`, `QueryCommand`,
`ManualCommand`), and `CommandFactory` names them. A verb opens a `Workspace` and runs its use case into the sink of
the technology asked for. `CommandLine` turns the outcome into the exit code and writes what it says on stderr.
`service::Outcome` now carries a `message`: `unknown-question`, `no-diagram`, `no-topic` and `errors` say what
happened. `ds manual` as text now goes through learn-language, whose page gains a presentation half. Check-design
names the workspace given, where its report says the open coverage is listed. Reading the disk is `app::Disk` and
writing a document is `app::Output`. `main` is app's only function, and a rule of the design holds it so. The free
functions of `ds/app/App.hpp` are gone.
