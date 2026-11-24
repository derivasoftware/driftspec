#include "ds/theme/Theme.hpp"

#include <stdexcept>

namespace ds {
namespace theme {

Theme::Theme()
    : tokens_{
      {"text", "#1e293b", "#e2e8f0"},
      {"muted", "#64748b", "#94a3b8"},
      {"stroke", "#64748b", "#94a3b8"},
      {"edge", "#64748b", "#94a3b8"},
      {"box-fill", "#ffffff", "#1e293b"},
      {"container-fill", "#f8fafc", "#0f172a"},
      {"container-stroke", "#94a3b8", "#64748b"},
      {"note-fill", "#fffbeb", "#3b3418"},
      {"note-stroke", "#fcd34d", "#a16207"},
      {"head-class", "#eef2ff", "#312e81"},
      {"head-interface", "#ecfdf5", "#064e3b"},
      {"head-enum", "#fff7ed", "#78350f"},
      {"head-function", "#f1f5f9", "#334155"},
      {"head-abstract", "#f5f3ff", "#4c1d95"},
      {"badge-class", "#4f46e5", "#a5b4fc"},
      {"badge-interface", "#059669", "#6ee7b7"},
      {"badge-enum", "#d97706", "#fcd34d"},
      {"badge-function", "#475569", "#cbd5e1"},
      {"badge-abstract", "#7c3aed", "#c4b5fd"},
      {"name", "#0f172a", "#f8fafc"},
      {"type", "#2563eb", "#7dd3fc"},
      {"param", "#475569", "#cbd5e1"},
      {"punct", "#94a3b8", "#64748b"},
      {"vis-public", "#16a34a", "#4ade80"},
      {"vis-private", "#dc2626", "#f87171"},
      {"vis-protected", "#d97706", "#fbbf24"},
      {"vis-package", "#2563eb", "#60a5fa"},
      {"port-in", "#2563eb", "#60a5fa"},
      {"port-out", "#c2730a", "#fbbf24"},
      {"focus", "#c2730a", "#fbbf24"},
      {"dimmed", ".25", ".3"},
      {"stroke-width", "1", ""},
      {"container-stroke-width", "1", ""},
      {"edge-width", "1", ""},
      {"edge-hit", "12", ""},
      {"added", "#15803d", "#4ade80"},
      {"removed", "#b91c1c", "#f87171"},
      {"changed", "#b45309", "#fbbf24"},
      {"renamed", "#1d4ed8", "#93c5fd"},
      {"frame-alt", "#4338ca", "#a5b4fc"},
      {"frame-loop", "#047857", "#6ee7b7"},
      {"frame-opt", "#b45309", "#fcd34d"},
      {"frame-other", "#334155", "#cbd5e1"},
      },
      font_{"\"JetBrains Mono\", \"Cascadia Code\", \"SFMono-Regular\", Menlo, Consolas, ui-monospace, monospace", 12, {8, 18}, {1, 1}},
      terminal_{
          {Role::muted, "2", "muted"},
          {Role::name, "1", ""},
          {Role::type, "34", "type"},
          {Role::publicMember, "32", "vis-public"},
          {Role::privateMember, "31", "vis-private"},
          {Role::protectedMember, "33", "vis-protected"},
          {Role::badge, "35", "badge-abstract"},
          {Role::note, "33", ""},
          {Role::container, "2", ""},
          {Role::edge, "", ""},
          {Role::added, "32", "added"},
          {Role::removed, "31", "removed"},
          {Role::renamed, "34", "renamed"},
          {Role::changed, "33", "changed"},
          {Role::against, "31", ""},
      } {
  for (const auto& t : terminal_) {  // a code takes the colour of the first role that names a token with it
    if (t.token.empty() || t.sgr.empty() || cellColours_.count(t.sgr)) continue;
    cellColours_[t.sgr] = static_cast<std::uint32_t>(std::stoul(token(t.token).light.substr(1), nullptr, 16));
  }
}

const Token& Theme::token(const std::string& name) const {
  for (const auto& t : tokens_)
    if (t.name == name) return t;
  throw std::logic_error("the theme has no token " + name);
}

const std::string& Theme::sgr(Role role) const {
  for (const auto& t : terminal_)
    if (t.role == role) return t.sgr;
  throw std::logic_error("the theme has no terminal colour for a role");
}

}  // namespace theme
}  // namespace ds
