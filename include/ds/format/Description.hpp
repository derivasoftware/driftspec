#ifndef DRIFTSPEC_FORMAT_DESCRIPTION_HPP
#define DRIFTSPEC_FORMAT_DESCRIPTION_HPP

#include "ds/emit/Sink.hpp"

namespace ds {
namespace format {

/// What is written, apart from how (DS-394, DS-398): a document describes itself once, as records of named
/// fields, lists and values, into the sink it is lent, and knows no technology that writes it. Whoever asks for
/// the output pairs a description (DescriptionFactory) with a sink (emit::SinkFactory) and takes the result; a
/// new document is one description, a new technology one sink, and neither touches the other. A sink that cannot
/// write what the description gives fails with emit::NotImplemented.
class IDescription {
 public:
  virtual ~IDescription() = default;
  /// The document as one value where the sink stands.
  virtual void describe(emit::ISink& out_sink) const = 0;
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DESCRIPTION_HPP
