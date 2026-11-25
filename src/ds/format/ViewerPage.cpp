#include "ds/format/ViewerPage.hpp"

#include <string>
#include <vector>

#include "ds/format/DescriptionFactory.hpp"

namespace ds {
namespace format {

// the viewer as one self-contained page with the scene inside, titled after the workspace
std::string ViewerPage::page(const scene::SceneDocument& document, const std::string& title, const std::string& css,
                             const std::vector<std::string>& focus, emit::ISink& out_sink) {
  DescriptionFactory::scene(document)->describe(out_sink);
  const std::string json =
      common::Packing::pack(out_sink.result());  // base64 holds no quote and no tag: nothing closes the script, nothing to escape
  std::string safe;
  for (const char c : title) safe += c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '&' ? "&amp;" : std::string(1, c);
  std::string out = viewerTemplate();
  const auto put = [&out](const std::string& mark, const std::string& value) {
    for (size_t at = out.find(mark); at != std::string::npos; at = out.find(mark, at + value.size())) out.replace(at, mark.size(), value);
  };
  std::string style = css;
  for (size_t at = style.find("</"); at != std::string::npos; at = style.find("</", at + 3)) style.replace(at, 2, "<\\/");
  put("/*SCENE*/", json);
  put("/*TITLE*/", safe);
  put("/*CSS*/", style);
  out_sink.beginList();
  for (const auto& id : focus) out_sink.value(id);
  out_sink.endList();
  std::string framed = out_sink.result();
  for (size_t at = framed.find("</"); at != std::string::npos; at = framed.find("</", at + 3)) framed.replace(at, 2, "<\\/");
  put("/*FOCUS*/", framed);
  // the viewer's code travels packed as well: the page carries the scene and the code as two packs
  // and a loader that inflates the code and runs it, so the page stays small as the viewer grows
  // (the scene and the focus stay outside the pack: they are what changes from one page to another)
  const std::string open = "<script>", close = "</script>";
  const std::string mark = "const PACKED='" + json + "';", aim = "const FOCUS=" + framed + ";";
  const size_t from = out.find(open, out.find("<body>")), to = out.rfind(close);  // a reader's stylesheet may hold "<script>"
  if (from == std::string::npos || to == std::string::npos || to < from) return out;
  std::string code = out.substr(from + open.size(), to - from - open.size());
  for (const std::string* cut : {&mark, &aim}) {
    const size_t at = code.find(*cut);
    if (at == std::string::npos) return out;
    code.erase(at, cut->size());
  }
  // the viewer's own styles travel packed too, and go in before the reader's (the theme), which wins
  const std::string styleOpen = "<style>", styleClose = "</style>";
  const size_t styleFrom = out.find(styleOpen), styleTo = out.find(styleClose, styleFrom);
  std::string own;
  if (styleFrom != std::string::npos && styleTo != std::string::npos && styleTo < from) {
    own = out.substr(styleFrom + styleOpen.size(), styleTo - styleFrom - styleOpen.size());
    out.erase(styleFrom, styleTo + styleClose.size() - styleFrom);
  }
  const size_t codeFrom = out.find(open, out.find("<body>")), codeTo = out.rfind(close);
  const std::string inflate =
      "const inflate=async p=>new Response(new Blob([Uint8Array.from(atob(p),c=>c.charCodeAt(0))]).stream()"
      ".pipeThrough(new DecompressionStream('deflate'))).text();";
  const std::string loader =
      "\n" + mark + "\n" + aim + "\nconst STYLE='" + common::Packing::pack(own) + "';\nconst VIEWER='" + common::Packing::pack(code) +
      "';\n(async()=>{try{" + inflate +
      "const st=document.createElement('style');st.textContent=await "
      "inflate(STYLE);document.head.insertBefore(st,document.getElementById('theme'));"
      "const t=await inflate(VIEWER);"
      "const s=document.createElement('script');s.textContent=t;document.body.appendChild(s);}"
      "catch(e){document.body.insertAdjacentHTML('beforeend','<p style=\"padding:20px;font-family:sans-serif\">"
      "This browser cannot inflate the viewer (it lacks DecompressionStream). Open the page in a current one.</p>');}})();\n";
  out.replace(codeFrom + open.size(), codeTo - codeFrom - open.size(), loader);
  return out;
}

// a page as an iframe that carries it whole (srcdoc), to paste into any HTML document
std::string ViewerPage::frame(const std::string& page, const std::string& title, int height) {
  const auto attribute = [](const std::string& text) {
    std::string out;
    out.reserve(text.size() + text.size() / 8);
    for (const char c : text) out += c == '&' ? "&amp;" : c == '"' ? "&quot;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : std::string(1, c);
    return out;
  };
  return "<iframe title=\"" + attribute(title) + " \u00b7 DriftSpec\" style=\"width:100%;height:" + std::to_string(height) +
         "px;border:0;border-radius:8px\" loading=\"lazy\" allowfullscreen srcdoc=\"" + attribute(page) + "\"></iframe>\n";
}

std::string ViewerPage::name() const { return "html"; }

std::string ViewerPage::write(const scene::SceneDocument& document, const FormatOptions& options, emit::ISink& out_sink) const {
  const std::string written = page(document, options.title, options.css, options.focus, out_sink);
  return options.embed ? frame(written, options.title, options.height) : written;
}

}  // namespace format
}  // namespace ds
