#include "ds/theme/Theme.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
      {"badge-interface", "#be185d", "#f9a8d4"},
      {"badge-enum", "#047857", "#6ee7b7"},
      {"badge-function", "#475569", "#cbd5e1"},
      {"badge-abstract", "#7c3aed", "#c4b5fd"},
      {"badge-dataclass", "#b45309", "#fcd34d"},
      {"badge-requirement", "#475569", "#cbd5e1"},
      {"badge-verification", "#059669", "#6ee7b7"},
      {"badge-constraint", "#d97706", "#fcd34d"},
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
      {"line-requirement", "", ""},   // the line under an artefact's name, in its kind's colour (DS-490): set below
      {"line-constraint", "", ""},
      {"line-usecase", "", ""},
      {"line-verification", "", ""},
      },
      pages_{
          {"viewer",
           {
               {"bg", "#f6f4ef", "#151618"},
               {"panel", "#fffdf8", "#1d1f22"},
               {"fg", "#1d1c1a", "#e8e6e1"},
               {"muted", "#6b675f", "#9a968d"},
               {"line", "#d6d0c3", "#3a3d42"},
               {"soft", "#ece8de", "#26292d"},
               {"accent", "#2f5d8a", "#7fb0e0"},
               {"accent-soft", "#d6e4f2", "#203a55"},
               {"req", "#8a4b2f", "#e3a27f"},
               {"req-soft", "#f3e3d8", "#3a271d"},
               {"priv", "#8c8579", "#857f75"},
               {"pkg", "#efe9dc", "#1a1c1f"},
               {"mod", "#f8f5ee", "#212427"},
               {"ifc", "#e3edf7", "#1c2a37"},
               {"k-class", "#4f46e5", "#a5b4fc"},
               {"k-function", "#475569", "#cbd5e1"},
               {"k-dataclass", "#b45309", "#fcd34d"},
               {"k-enum", "#047857", "#6ee7b7"},
               {"k-interface", "#be185d", "#f9a8d4"},
               {"k-abstract", "#7c3aed", "#c4b5fd"},
               {"ok", "#2f8a4a", "#6cc68a"},
               {"warn", "#d9822b", "#f0a35a"},
               {"bad", "#c0392b", "#f07a6a"},
               {"r-owns", "#c2185b", "#f06292"},
               {"r-agg", "#00838f", "#4dd0e1"},
               {"r-uses", "#5c6bc0", "#9fa8da"},
               {"r-calls", "#c62828", "#ef7f7f"},
               {"you", "#2f62c9", ""},
               {"shadow", "rgba(0,0,0,.18)", ""},
               {"shadow-pop", "rgba(0,0,0,.2)", ""},
               {"shadow-soft", "rgba(0,0,0,.12)", ""},
           }},
          {"viewer-night",
           {
               {"bg", "#0e1116", ""},
               {"panel", "#161b22", ""},
               {"fg", "#e6edf3", ""},
               {"muted", "#8b949e", ""},
               {"line", "#30363d", ""},
               {"soft", "#1f242c", ""},
               {"accent", "#58a6ff", ""},
               {"accent-soft", "#1b2b44", ""},
               {"req", "#f0a07a", ""},
               {"req-soft", "#3a2418", ""},
               {"priv", "#7d8590", ""},
               {"pkg", "#12161c", ""},
               {"mod", "#181d24", ""},
               {"ifc", "#15263a", ""},
               {"k-class", "#a5b4fc", ""},
               {"k-function", "#cbd5e1", ""},
               {"k-dataclass", "#fcd34d", ""},
               {"k-enum", "#6ee7b7", ""},
               {"k-interface", "#f9a8d4", ""},
               {"k-abstract", "#c4b5fd", ""},
               {"r-owns", "#f778ba", ""},
               {"r-agg", "#39c5cf", ""},
               {"r-uses", "#a5b4fc", ""},
               {"r-calls", "#ff7b72", ""},
               {"ok", "#3fb950", ""},
               {"warn", "#f0883e", ""},
               {"bad", "#ff7b72", ""},
               {"you", "#2f62c9", ""},
               {"shadow", "rgba(0,0,0,.18)", ""},
               {"shadow-pop", "rgba(0,0,0,.2)", ""},
               {"shadow-soft", "rgba(0,0,0,.12)", ""},
           }},
          {"live",
           {
               {"bg", "#f4f6f8", "#12161b"},
               {"panel", "#fff", "#1a2027"},
               {"fg", "#1b2129", "#e4e9ef"},
               {"muted", "#5a6573", "#9aa6b4"},
               {"line", "#d5dbe2", "#2f3943"},
               {"accent", "#d9822b", "#f0a35a"},
               {"ok", "#2f8a4a", "#6cc68a"},
               {"err", "#c2410c", "#f08a5d"},
               {"warn", "#a16207", "#e3b341"},
               {"info", "#0e7490", "#5cc8d9"},
               {"sel", "#2f62c9", "#7aa2f2"},
               {"on-sel", "#fff", ""},
           }},
          {"playground",
           {
               {"page", "#f7f7f7", "#111111"},
               {"bg", "#eeeeee", "#171717"},
               {"panel", "#ffffff", "#202020"},
               {"raised", "#ffffff", "#2a2a2a"},
               {"line", "#dcdcdc", "#333333"},
               {"rule", "#9a9a9a", "#5a5a5a"},
               {"fg", "#1d1d1d", "#e9e9e9"},
               {"muted", "#6a6a6a", "#929292"},
               {"faint", "#9e9e9e", "#676767"},
               {"soft", "#f2f2f2", "#2a2a2a"},
               {"accent", "#1d1d1d", "#e9e9e9"},
               {"c-req", "#168a4d", "#4fd08a"},
               {"c-cst", "#7a3fd6", "#b58cff"},
               {"c-rule", "#a47a00", "#f0cd3c"},
               {"c-seq", "#1f6fd6", "#5aa9ff"},
               {"c-state", "#0b8794", "#36c9d6"},
               {"c-fail", "#cf4a14", "#ff8552"},
               {"k-class", "#4f46e5", "#a5b4fc"},
               {"k-interface", "#be185d", "#f9a8d4"},
               {"k-dataclass", "#b45309", "#fcd34d"},
               {"k-enum", "#047857", "#6ee7b7"},
               {"k-function", "#475569", "#cbd5e1"},
               {"k-abstract", "#7c3aed", "#c4b5fd"},
               {"s-fn", "#795e26", "#dcdcaa"},
               {"s-pn", "#001080", "#9cdcfe"},
               {"s-ty", "#267f99", "#4ec9b0"},
               {"s-kw", "#af00db", "#c586c0"},
               {"s-str", "#a31515", "#ce9178"},
               {"v-pub", "#1e8e3e", "#56c271"},
               {"v-prot", "#a67c00", "#e6c84a"},
               {"v-priv", "#d1343f", "#f0616d"},
               {"r-inherits", "#6b46c1", "#c9a8ff"},
               {"r-has", "#9a6b00", "#ffd166"},
               {"r-refers", "#0f766e", "#6fd6c4"},
               {"r-uses", "#1d5fbf", "#8fbcff"},
               {"brand", "#6B3FB8", "#B79CF0"},
               {"on-brand", "#ffffff", "#151618"},
               {"coach", "#6b3fb8", "#b79cf0"},
               {"on-coach", "#f6f4ef", "#151618"},
               {"shadow-card", "rgba(0,0,0,.22)", "rgba(0,0,0,.55)"},
               {"shadow-float", "", "rgba(0,0,0,.35)"},
               {"eye", "", "#fff"},
               {"pupil", "", "#151618"},
           }},
          {"design-page",
           {
               {"bg", "#eef1ee", "#0f1614"},
               {"panel", "#fbfcfa", "#161f1c"},
               {"ink", "#18211e", "#e4ebe7"},
               {"muted", "#5d6b65", "#93a39c"},
               {"rule", "#cfd7d1", "#2b3833"},
               {"accent", "#0b6e69", "#58c4b8"},
               {"accent-soft", "#d5ebe7", "#1b3a36"},
               {"mark", "#fdf0c4", "#4a3f14"},
               {"warn", "#a8530a", "#e5a15e"},
               {"kw", "#7a3e9d", "#c79bf0"},
               {"ty", "#0b6e69", "#58c4b8"},
               {"id", "#a8530a", "#e5a15e"},
               {"cm", "#7a8880", "#7d8d86"},
               {"st", "#2f6f3e", "#8fce9d"},
               {"ar", "#5d6b65", "#93a39c"},
           }},
      },
      font_{"\"JetBrains Mono\", \"Cascadia Code\", \"SFMono-Regular\", Menlo, Consolas, ui-monospace, monospace", 12, {8, 18, 7}, {1, 1, 1}},
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
          {Role::classBadge, "34", ""},
          {Role::interfaceBadge, "35", ""},
          {Role::enumBadge, "32", ""},
          {Role::dataclassBadge, "33", ""},
          {Role::functionBadge, "2", ""},
          {Role::abstractBadge, "35", ""},
          {Role::requirementBadge, "35", ""},
          {Role::verificationBadge, "32", ""},
          {Role::constraintBadge, "33", ""},
      } {
  drawnAsThePlayground();
  for (const auto& t : terminal_) {  // a code takes the colour of the first role that names a token with it
    if (t.token.empty() || t.sgr.empty() || cellColours_.count(t.sgr)) continue;
    cellColours_[t.sgr] = static_cast<std::uint32_t>(std::stoul(token(t.token).light.substr(1), nullptr, 16));
  }
}

namespace {
/// a colour `share` of the way from `under` to `over`, as CSS's color-mix(in srgb, over share%, under) gives it
std::string mixed(const std::string& over, const std::string& under, double share) {
  const auto rgb = [](const std::string& hex) {
    const std::uint32_t v = static_cast<std::uint32_t>(std::stoul(hex.substr(1), nullptr, 16));
    return std::vector<double>{double(v >> 16 & 0xff), double(v >> 8 & 0xff), double(v & 0xff)};
  };
  const auto a = rgb(over), b = rgb(under);
  char out[8];
  std::snprintf(out, sizeof out, "#%02x%02x%02x", int(a[0] * share + b[0] * (1 - share) + .5), int(a[1] * share + b[1] * (1 - share) + .5),
                int(a[2] * share + b[2] * (1 - share) + .5));
  return out;
}
}  // namespace

// a drawing looks as the playground draws it (DS-490): each of its tokens is the playground's colour its own rules gave
// that token, so the playground's palette is the one source of both and a drawing carries the look on any page
void Theme::drawnAsThePlayground() {
  const std::vector<Token>& p = pages_.at("playground");
  const auto of = [&p](const std::string& name) -> const Token& {
    for (const auto& t : p)
      if (t.name == name) return t;
    throw std::logic_error("the playground's palette has no token " + name);
  };
  const std::vector<std::pair<std::string, std::string>> as{
      {"text", "fg"},
      {"muted", "muted"},
      {"stroke", "rule"},
      {"edge", "rule"},
      {"box-fill", "panel"},
      {"container-stroke", "line"},
      {"note-fill", "raised"},
      {"note-stroke", "line"},
      {"head-class", "panel"},
      {"head-interface", "panel"},
      {"head-enum", "panel"},
      {"head-function", "panel"},
      {"head-abstract", "panel"},
      {"badge-class", "k-class"},
      {"badge-interface", "k-interface"},
      {"badge-enum", "k-enum"},
      {"badge-function", "muted"},
      {"badge-abstract", "k-class"},
      {"badge-dataclass", "k-dataclass"},
      {"name", "fg"},
      {"type", "s-ty"},
      {"param", "s-pn"},
      {"punct", "muted"},
      {"vis-public", "v-pub"},
      {"vis-protected", "v-prot"},
      {"vis-private", "v-priv"},
      {"vis-package", "muted"},
      {"line-requirement", "c-req"},
      {"line-constraint", "c-cst"},
      {"line-usecase", "c-state"},
      {"line-verification", "c-seq"},
  };
  for (const auto& a : as)
    for (auto& t : tokens_)
      if (t.name == a.first) {
        const Token& from = of(a.second);
        t.light = from.light;
        t.dark = from.dark;
      }
  // a container: the panel 45% over the background, as the playground mixes it
  for (auto& t : tokens_)
    if (t.name == "container-fill") {
      t.light = mixed(of("panel").light, of("bg").light, .45);
      t.dark = mixed(of("panel").dark, of("bg").dark, .45);
    }
}

const Token& Theme::token(const std::string& name) const {
  for (const auto& t : tokens_)
    if (t.name == name) return t;
  throw std::logic_error("the theme has no token " + name);
}

const std::vector<Token>& Theme::page(const std::string& name) const {
  const auto p = pages_.find(name);
  if (p == pages_.end()) throw std::logic_error("the theme has no page " + name);
  return p->second;
}

const std::string& Theme::sgr(Role role) const {
  for (const auto& t : terminal_)
    if (t.role == role) return t.sgr;
  throw std::logic_error("the theme has no terminal colour for a role");
}

std::string Theme::filled(const std::string& stylesheet, const std::vector<Token>& tokens) const {
  const Font& font = font_;
  std::string out;
  out.reserve(stylesheet.size() + stylesheet.size() / 4);
  std::size_t at = 0;
  while (true) {
    const std::size_t open = stylesheet.find("{{", at);
    if (open == std::string::npos) break;
    const std::size_t close = stylesheet.find("}}", open);
    if (close == std::string::npos) throw std::logic_error("a placeholder of the stylesheet is not closed");
    out.append(stylesheet, at, open - at);
    std::string name = stylesheet.substr(open + 2, close - open - 2);
    if (name == "font-family") {
      out += font.family;
    } else if (name == "font-size") {
      out += std::to_string(font.sizePx);
    } else {
      const bool dark = name.size() > 5 && name.compare(name.size() - 5, 5, ":dark") == 0;
      if (dark) name.resize(name.size() - 5);
      const Token* token = nullptr;
      for (const auto& t : tokens)
        if (t.name == name) token = &t;
      if (token == nullptr) throw std::logic_error("the theme has no token " + name);
      out += dark && !token->dark.empty() ? token->dark : token->light;
    }
    at = close + 2;
  }
  out.append(stylesheet, at, std::string::npos);
  return out;
}

}  // namespace theme
}  // namespace ds
