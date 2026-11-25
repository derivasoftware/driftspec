#include "ds/api/Browser.hpp"

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/service/UseCase.hpp"

namespace ds {
namespace api {

// the playground's door (DS-460): the request read, the design (or the manual) answered by the use case play-design
// through the workspace, as every door asks the engine; it draws nothing and reads no model
void Browser::respond(const std::string& request, emit::ISink& out_sink) const {
  const auto in = nlohmann::json::parse(request);  // the request is read; the answer is written by the sink
  service::Request asked;
  Files files;
  if (in.contains("manual")) {  // a page of the manual, or with "" its guides and topics (DS-272)
    asked.arguments = {{"part", "manual"}, {"topic", in["manual"].get<std::string>()}};
  } else {
    for (const auto& f : in.at("files").items()) files[f.key()] = f.value().get<std::string>();
    asked.arguments = {{"part", "design"}, {"width", std::to_string(in.value("width", 0))}};
  }
  Workspace workspace = Workspace::madeFor(metamodel_, files);
  workspace.run("play-design", asked, out_sink);
}

std::string Browser::answer(const std::string& request) const {
  // the answer is written by a JSON sink asked for here, at the engine's edge, and taken whole (DS-397)
  try {
    const auto sink = emit::SinkFactory::json(false);
    respond(request, *sink);
    return sink->result();
  } catch (const std::exception& ex) {
    const auto sink = emit::SinkFactory::json(false);
    sink->beginRecord().field("error").value(ex.what()).endRecord();
    return sink->result();
  }
}

}  // namespace api
}  // namespace ds
