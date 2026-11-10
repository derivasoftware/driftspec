#ifndef DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP
#define DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP

#include <map>
#include <memory>
#include <string>

#include "ds/format/Format.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace format {

/// The IR itself, as its contract writes it; indented when asked (ds render -f ir).
class DiagramIr : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options) const override;
};

/// Where each node and route goes, in cells: the portable form for hosts that draw themselves (-f layout).
class DiagramLayout : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options) const override;
};

/// The diagram laid out and drawn as SVG (-f svg).
class DiagramSvg : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options) const override;
};

/// The diagram in characters, for a terminal (-f text).
class DiagramText : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options) const override;
};

/// The diagram as the terminal's coloured cells (-f cells).
class DiagramCells : public IFormat<render::IrText> {
 public:
  std::string name() const override;
  std::string write(const render::IrText& document, const FormatOptions& options) const override;
};

/// The one place that names the formats of a diagram, keyed by name.
std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> diagramFormats();

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DIAGRAMFORMATS_HPP
