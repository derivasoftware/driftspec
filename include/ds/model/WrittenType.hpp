#ifndef DRIFTSPEC_MODEL_WRITTENTYPE_HPP
#define DRIFTSPEC_MODEL_WRITTENTYPE_HPP

#include <string>

namespace ds {
namespace model {

/// A type as the design writes it (DS-390): std::vector<model::Entity>, the code's text, nothing resolved. What a box
/// shows and what a contract compares are read from it.
class WrittenType {
 public:
  explicit WrittenType(std::string text);
  /// The type without the names that qualify it: std::vector<model::Entity> is vector<Entity>. A name qualifies when
  /// it starts a word with a letter or _ and :: follows it.
  std::string unqualified() const;

 private:
  /// A character of a word: a letter, a digit or _.
  static bool inWord(char c);
  /// A character a name may start with: a letter or _.
  static bool startsName(char c);

  std::string text_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_WRITTENTYPE_HPP
