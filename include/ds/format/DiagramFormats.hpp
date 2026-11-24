#ifndef DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP
#define DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP

#include <map>
#include <memory>
#include <string>

#include "ds/emit/Sink.hpp"
#include "ds/format/Format.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace format {

/// The IR itself, as its contract writes it; indented when asked (ds render -f ir).
class DiagramIr : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// Where each node and route goes, in cells: the portable form for hosts that draw themselves (-f layout).
class DiagramLayout : public IFormat<render::IrText> {
 public:
  /// In cells, measured as the theme measures text in a terminal (DS-393).
  explicit DiagramLayout(const theme::Theme& theme);
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const override;

 private:
  /// The placed rectangles and routes of one drawing, in cells, as one record; of a drawing made of parts, those
  /// of each part; `part`, when given, is the name of the part it is.
  void placed(const render::IrDocument& document, int width, const std::string* part, emit::ISink& out_sink) const;

  const render::Metrics metrics_;
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

/// The diagram as the terminal's coloured cells (-f cells).
class DiagramCells : public IFormat<render::IrText> {
 public:
  explicit DiagramCells(const theme::Theme& theme) : text_(theme) {}
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const override;

 private:
  const render::Text text_;
};

/// The one place that names the formats of a diagram, keyed by name; those that draw draw in `theme` (DS-393).
std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> diagramFormats(const theme::Theme& theme);

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP
