// The engine behind the page, one call: ds(files, width) -> the document of ds::api::Browser
// (REQ-00113-1), from ds.js + ds.wasm built beside the page (playground/build.py).

let call = null;

export async function start() {
  const { default: createDs } = await import("./ds.js");
  const mod = await createDs();
  call = (request) => mod.ccall("ds_playground", "string", ["string"], [request]);
  return "wasm";
}

export async function ds(files, width = 0) {
  const out = JSON.parse(call(JSON.stringify({ files, width })));
  // what is not yet written is no problem (DS-216): the page lists it as information, what is left to write
  if (out.report) out.report.findings = out.report.findings.concat((out.report.coverage || []).map((f) => ({ ...f, severity: "info" })));
  return out;
}

// A page of the manual the engine carries, or with "" its guides and topics (DS-272): the same text ds manual prints.
export function manual(topic = "") {
  return JSON.parse(call(JSON.stringify({ manual: topic }))).manual;
}
