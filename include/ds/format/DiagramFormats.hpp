#ifndef DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP
#define DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP

#include <string>

#include "ds/emit/Sink.hpp"
#include "ds/format/Format.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace format {

/// The writers of a diagram that are no description yet (DS-398): FormatFactory names them.

/// The IR itself, as its contract writes it; indented when asked (ds render -f ir).
class DiagramIr : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// The diagram laid out and drawn as SVG (-f svg).
class DiagramSvg : public IFormat<render::IrText> {
 public:
  explicit DiagramSvg(const theme::Theme& theme) : svg_(theme) {}
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const override;

 private:
  const render::Svg svg_;
};

/// The diagram in characters, for a terminal (-f text).
class DiagramText : public IFormat<render::IrText> {
 public:
  explicit DiagramText(const theme::Theme& theme) : text_(theme) {}
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const override;

 private:
  const render::Text text_;
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP
