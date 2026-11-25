#ifndef DRIFTSPEC_EMIT_NOTIMPLEMENTED_HPP
#define DRIFTSPEC_EMIT_NOTIMPLEMENTED_HPP

#include <stdexcept>
#include <string>
#include <vector>

namespace ds {
namespace emit {

/// A document asked in a technology that does not write it (DS-394, DS-398): the documents and the technologies
/// need not make a complete matrix. The one asked for a pair it has not throws it, naming the document and the
/// technology and what it has instead; whoever asked for the output says it as bad usage.
class NotImplemented : public std::logic_error {
 public:
  /// `document` is empty when nothing at all is written in `technology`; `there` is what is written instead.
  NotImplemented(const std::string& document, const std::string& technology, const std::vector<std::string>& there);
  const std::string& document() const;
  const std::string& technology() const;

 private:
  /// What the failure says: the document, or nothing, not written in the technology, and what there is.
  static std::string said(const std::string& document, const std::string& technology, const std::vector<std::string>& there);

  std::string document_;    ///< what was asked to be written; empty for any document
  std::string technology_;  ///< what it was asked to be written in
};

}  // namespace emit
}  // namespace ds

#endif  // DRIFTSPEC_EMIT_NOTIMPLEMENTED_HPP
