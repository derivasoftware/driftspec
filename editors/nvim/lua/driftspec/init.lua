--- driftspec.nvim: the DriftSpec language in Neovim.
--- Self-sufficient: `make` in editors/nvim compiles the grammar of this very
--- repository into parser/driftspec.so and copies its queries; setup() wires
--- the filetype, native tree-sitter highlighting and folding. No
--- nvim-treesitter required. The language server arrives with `ds lsp`.

local M = {}

M.defaults = { folds = true }

local function plugin_root()
  local source = debug.getinfo(1, "S").source:sub(2)
  return vim.fn.fnamemodify(source, ":h:h:h")
end

function M.setup(opts)
  opts = vim.tbl_deep_extend("force", M.defaults, opts or {})
  local root = plugin_root()
  vim.filetype.add({ extension = { ds = "driftspec" } })
  -- the compiled parser and the queries live in this plugin's runtimepath
  vim.opt.runtimepath:append(root)
  vim.treesitter.language.register("driftspec", "driftspec")
  local function attach(buf)
      local ok = pcall(vim.treesitter.start, buf, "driftspec")
      if not ok then
        vim.notify("driftspec: parser not built; run `make` in " .. root, vim.log.levels.WARN)
        return
      end
      vim.bo[buf].commentstring = "// %s"
      if opts.folds then
        vim.wo.foldmethod = "expr"
        vim.wo.foldexpr = "v:lua.vim.treesitter.foldexpr()"
        vim.wo.foldlevel = 99
      end
  end
  vim.api.nvim_create_autocmd("FileType", { pattern = "driftspec", callback = function(ev) attach(ev.buf) end })
  -- the plugin is usually lazy-loaded BY the filetype, so the event already fired for this buffer
  for _, buf in ipairs(vim.api.nvim_list_bufs()) do
    if vim.bo[buf].filetype == "driftspec" then attach(buf) end
  end
end

return M
