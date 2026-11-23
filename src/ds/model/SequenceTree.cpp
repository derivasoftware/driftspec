#include "ds/model/SequenceTree.hpp"

#include "ds/model/Identity.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace model {

bool SequenceTree::isBranch(const Entity& e) { return e.kind == Kind::Frame && e.field("kind") == "branch"; }

SequenceTree::SequenceTree(const Model& model, const std::string& sequence) : model_(model) { walk(sequence, ""); }

// What the sequence or a frame holds, in the order written: a message takes the next place; a frame
// opens before what it holds; a branch is a side of the frame around it, never a frame of its own.
void SequenceTree::walk(const std::string& holder, const std::string& frame) {
  for (const auto& id : model_.index().children(holder)) {
    const Entity* step = model_.find(id);
    if (!step) continue;
    frameOf_[id] = frame;
    // where it lies on the else side of a frame is told here, once, instead of by a walk up for each question (DS-345)
    for (const auto& f : sides_) onElse_.insert({f, id});
    if (step->kind == Kind::Message) {
      order_[id] = static_cast<int>(messages_.size());
      messages_.push_back(id);
      for (std::string f = frame; !f.empty() && first_[f].empty(); f = frameOf_[f]) first_[f] = id;
      for (const auto& f : sides_) elseFirst_.emplace(f, id);
    } else if (step->kind == Kind::Frame && isBranch(*step)) {
      const bool otherwise = id == Identity(frame).branch(true).text();
      if (otherwise) sides_.push_back(frame);
      walk(id, frame);  // its steps are the alt's, on one side
      if (otherwise) sides_.pop_back();
    } else if (step->kind == Kind::Frame) {
      frames_.push_back(id);
      at_[id] = static_cast<int>(messages_.size());
      first_[id] = "";
      walk(id, id);
    }
  }
}

int SequenceTree::orderOf(const std::string& message) const {
  const auto found = order_.find(message);
  return found == order_.end() ? -1 : found->second;
}

std::string SequenceTree::frameOf(const std::string& id) const {
  const auto found = frameOf_.find(id);
  return found == frameOf_.end() ? "" : found->second;
}

int SequenceTree::at(const std::string& frame) const {
  const auto found = at_.find(frame);
  return found == at_.end() ? -1 : found->second;
}

std::string SequenceTree::first(const std::string& frame) const {
  const auto found = first_.find(frame);
  return found == first_.end() ? "" : found->second;
}

std::string SequenceTree::elseOf(const std::string& frame) const {
  const auto found = elseFirst_.find(frame);
  return found == elseFirst_.end() ? "" : found->second;
}

bool SequenceTree::twoSides(const std::string& frame) const {
  const Entity* otherwise = model_.find(Identity(frame).branch(true).text());
  return otherwise && (!elseOf(frame).empty() || !otherwise->field("scenario").empty());
}

bool SequenceTree::onElse(const std::string& frame, const std::string& id) const { return onElse_.count({frame, id}) > 0; }

std::string SequenceTree::scenario(const std::string& frame, bool elseSide) const {
  const Entity* f = model_.find(frame);
  if (!f) return "";
  if (f->field("kind") != "alt") return elseSide ? "" : f->field("scenario");
  const Entity* side = model_.find(Identity(frame).branch(elseSide).text());
  return side ? side->field("scenario") : "";
}

std::vector<std::string> SequenceTree::inside(const std::string& frame, bool elseSide) const {
  std::vector<std::string> out;
  for (const auto& f : frames_)
    if (frameOf(f) == frame && onElse(frame, f) == elseSide) out.push_back(f);
  return out;
}

}  // namespace model
}  // namespace ds
