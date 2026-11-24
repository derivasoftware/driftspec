#include <miniz.h>

#include <cstring>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "ds/format/Format.hpp"
#include "ds/model/Identity.hpp"
#include "ds/scene/Map.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace format {
namespace common {

std::string pack(const std::string& text) {
  mz_ulong size = mz_compressBound(static_cast<mz_ulong>(text.size()));
  std::vector<unsigned char> out(size);
  if (mz_compress2(out.data(), &size, reinterpret_cast<const unsigned char*>(text.data()), static_cast<mz_ulong>(text.size()),
                   MZ_BEST_COMPRESSION) != MZ_OK)
    return "";
  return text::base64(out.data(), size);
}

std::string unpack(const std::string& packed) {
  const std::string bytes = text::fromBase64(packed);
  for (mz_ulong room = static_cast<mz_ulong>(bytes.size()) * 8 + 1024; room < (1ul << 30); room *= 2) {
    std::vector<unsigned char> out(room);
    mz_ulong size = room;
    const int status =
        mz_uncompress(out.data(), &size, reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<mz_ulong>(bytes.size()));
    if (status == MZ_OK) return std::string(out.begin(), out.begin() + static_cast<long>(size));
    if (status != MZ_BUF_ERROR) return "";
  }
  return "";
}

// The document is compact, and says how to read itself: every record is an array whose
// columns "fields" names, and a node, a behaviour or a link is named by its index.
std::string encode(const scene::SceneDocument& doc, bool pretty) {
  using namespace scene;  // the document's records, and the map that says where an identity lands
  std::map<std::string, int> at;
  for (size_t i = 0; i < doc.nodes.size(); ++i) at[doc.nodes[i].id] = static_cast<int>(i);
  const Map landing(doc.nodes);  // where a link's sequence lands: the behaviour it is
  const auto index = [&at](const std::string& id) { return at.count(id) ? at.at(id) : -1; };
  const auto ints = [](const layout::Box& b) {
    return nlohmann::json::array({std::lround(b.x), std::lround(b.y), std::lround(b.w), std::lround(b.h)});
  };
  std::map<std::string, int> behaviourOf;  // "subject / scenario" -> its index
  for (size_t i = 0; i < doc.behaviours.size(); ++i) behaviourOf[doc.behaviours[i].id] = static_cast<int>(i);

  nlohmann::json out;
  out["version"] = 2;
  out["fields"] = {{"node", {"id", "kind", "parent", "label", "stereotype", "visibility", "requirements", "full", "lean"}},
                   {"link", {"from", "to", "kind", "count", "multiplicity", "behaviours"}},
                   {"behaviour", {"subject", "scenario", "calls"}},
                   {"call", {"to", "label", "behaviour", "result"}},
                   {"tour", {"behaviour", "group"}},
                   {"diagram", {"name", "kind", "nodes"}},
                   {"requirement", {"id", "kind", "title", "status", "text", "parent", "scope", "nodes", "signature", "outcomes", "binds"}},
                   {"verification", {"id", "title", "status", "method", "tests", "requirements"}},
                   {"rule", {"id", "kind", "name", "title", "scope", "lines", "requirements", "breaches", "file", "line"}},
                   {"breach", {"node", "to", "bindings", "witness", "file", "line"}}};
  out["nodes"] = nlohmann::json::array();
  for (const auto& n : doc.nodes) {
    out["nodes"].push_back({n.id, n.kind, index(n.parent), n.label, n.stereotype, n.visibility, n.requirements, ints(n.full),
                            n.lean ? ints(n.leanBox) : nlohmann::json()});
  }
  out["world"] = ints(doc.world);
  out["lean_world"] = ints(doc.leanWorld);
  out["links"] = nlohmann::json::array();
  for (const auto& l : doc.links) {
    nlohmann::json written = nlohmann::json::array();
    for (const auto& s : l.sequences) {  // a sequence id, X::sequence or X::sequence/scenario, as the behaviour it is
      const std::string scenario = model::Identity(s).scenario();
      const auto b = behaviourOf.find(landing.land(model::Identity(s).subject().text()) + (scenario.empty() ? "" : " / " + scenario));
      if (b != behaviourOf.end()) written.push_back(b->second);
    }
    out["links"].push_back({index(l.from), index(l.to), l.kind, l.count, l.multiplicity, written});
  }
  out["behaviours"] = nlohmann::json::array();
  for (const auto& b : doc.behaviours) {
    nlohmann::json calls = nlohmann::json::array();
    for (const auto& c : b.calls) calls.push_back({index(c.to), c.label, c.expands, c.result});
    out["behaviours"].push_back({index(b.subject), b.scenario, calls});
  }
  out["tours"] = nlohmann::json::array();
  for (const auto& t : doc.tours) out["tours"].push_back({t.behaviour, t.group});
  out["diagrams"] = nlohmann::json::array();
  for (const auto& d : doc.diagrams) {
    nlohmann::json shown = nlohmann::json::array();
    for (const auto& n : d.nodes) shown.push_back(index(n));
    out["diagrams"].push_back({d.name, d.kind, shown});
  }
  out["titles"] = doc.titles;
  std::map<std::string, int> requirementAt;
  for (size_t i = 0; i < doc.requirements.size(); ++i) requirementAt[doc.requirements[i].id] = static_cast<int>(i);
  const auto requirement = [&requirementAt](const std::string& id) { return requirementAt.count(id) ? requirementAt.at(id) : -1; };
  out["requirements"] = nlohmann::json::array();
  for (const auto& r : doc.requirements) {
    nlohmann::json shown = nlohmann::json::array();
    for (const auto& n : r.nodes) shown.push_back(index(n));
    nlohmann::json ends = nlohmann::json::array();
    for (const auto& o : r.outcomes) ends.push_back({o.name, o.returns, o.text});
    nlohmann::json bound = nlohmann::json::array();
    for (const auto& b : r.binds)
      if (requirement(b) >= 0) bound.push_back(requirement(b));
    out["requirements"].push_back(
        {r.id, r.kind, r.title, r.status, r.text, requirement(r.parent), index(r.scope), shown, r.signature, ends, bound});
  }
  out["verifications"] = nlohmann::json::array();
  for (const auto& v : doc.verifications) {
    nlohmann::json proves = nlohmann::json::array();
    for (const auto& r : v.requirements)
      if (requirement(r) >= 0) proves.push_back(requirement(r));
    out["verifications"].push_back({v.id, v.title, v.status, v.method, v.tests, proves});
  }
  out["rules"] = nlohmann::json::array();
  for (const auto& r : doc.rules) {
    nlohmann::json broken = nlohmann::json::array();
    for (const auto& b : r.breaches) broken.push_back({index(b.node), index(b.to), b.bindings, b.witness, b.file, b.line});
    nlohmann::json lines = nlohmann::json::array();
    for (const auto& l : r.lines) lines.push_back({l.first, l.second});
    out["rules"].push_back({r.id, r.kind, r.name, r.title, index(r.scope), lines, r.requirements, broken, r.file, r.line});
  }
  return pretty ? out.dump(2) : out.dump();
}

}  // namespace common
}  // namespace format
}  // namespace ds
