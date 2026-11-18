# driftspec.nvim

The DriftSpec language in Neovim (0.10 or later): the filetype, highlighting and folding from the
grammar of this repository, and the language server when it is installed.

```bash
make -C editors/nvim            # compiles grammar/ into parser/driftspec.so and copies the queries
pip install ".[lsp]"            # optional, from the root of the repository: driftspec-lsp, the language server
```

```lua
-- lazy.nvim, or any manager that adds the folder to the runtimepath
{ dir = "path/to/driftspec/editors/nvim", ft = "driftspec",
  config = function() require("driftspec").setup({ folds = true, lsp = true }) end }
```

The server checks the design a file belongs to on every change and gives diagnostics, go to
definition, references, hover, completion and symbols.
