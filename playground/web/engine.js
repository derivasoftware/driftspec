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
  return JSON.parse(call(JSON.stringify({ files, width })));
}
