#ifndef DRIFTSPEC_THEME_WRITERS_HPP
#define DRIFTSPEC_THEME_WRITERS_HPP

#include <map>
#include <memory>
#include <string>

#include "ds/theme/Theme.hpp"

namespace ds {
namespace theme {

/// What writes the theme in one form a drawing or a page reads (DS-393): it holds where the values go, never a value.
class IThemeWriter {
 public:
  virtual ~IThemeWriter() = default;
  virtual std::string write(const Theme& theme) const = 0;
};

/// The stylesheet every SVG carries: the --pr-* contract of plantuml-render, light, and dark twice (for a page that
/// follows the system and for one that sets data-theme), then the rules that read the tokens.
class SvgStyleSheet : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// What a sequence's SVG adds to it: lifelines, rows, frames and their tokens.
class SequenceStyleSheet : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// The viewer page's variables (viewer/scene.html): its light palette and its dark one.
class ViewerVariables : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// The viewer's night theme (viewer/themes/night.css), a stylesheet a reader lays over the viewer.
class ViewerNightVariables : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// The live page's variables (python/driftspec/live_page.py): light, and dark when the system is.
class LivePageVariables : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// The playground's variables (playground/web/index.html): dark first, and light when a page sets it.
class PlaygroundVariables : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// The design page's variables (tools/design_page.py): light, and dark twice, as the SVG's.
class DesignPageVariables : public IThemeWriter {
 public:
  std::string write(const Theme& theme) const override;

 private:
  static const char* const kStylesheet;  ///< where each value goes; the values are the theme's (DS-477)
};

/// The one place that names the writers, by the form they write: "svg", "sequence", and each page by its name.
class ThemeWriterFactory {
 public:
  static std::map<std::string, std::unique_ptr<IThemeWriter>> of();
};

}  // namespace theme
}  // namespace ds

#endif  // DRIFTSPEC_THEME_WRITERS_HPP
