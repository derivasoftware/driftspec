#ifndef DRIFTSPEC_FORMAT_DESCRIPTION_HPP
#define DRIFTSPEC_FORMAT_DESCRIPTION_HPP

#include <string>

#include "ds/emit/Sink.hpp"

namespace ds {
namespace format {

/// What is written, apart from how (DS-394, DS-398, DS-399): a document describes itself once into the sink it is
/// lent, as structure (records of named fields, lists and values) and as presentation (paragraphs and tables), and
/// knows no technology that writes it. Whoever asks for the output pairs a description (DescriptionFactory) with a
/// sink (emit::SinkFactory) and takes the result; the sink writes the half it can. A new document is one
/// description, a new technology one sink, and neither touches the other. Abstract, so that every description ends
/// the same way: the sink is told which document it was handed, and one that accepted nothing of it fails with
/// emit::NotImplemented.
class ADescription {
 public:
  virtual ~ADescription() = default;
  /// The document as one value where the sink stands, said in both halves; emit::NotImplemented, naming the
  /// document and the technology, when the sink writes neither.
  void describe(emit::ISink& out_sink) const;
  /// The name of the document: scene, report, answers, layout or cells.
  virtual std::string name() const = 0;

 private:
  /// What the document says, its structure and its presentation, into the sink lent.
  virtual void say(emit::ISink& out_sink) const = 0;
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DESCRIPTION_HPP
