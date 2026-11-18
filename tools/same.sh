#!/usr/bin/env bash
# Compare what two builds of ds write, byte for byte (doc/design-practices.md: a refactor proves it
# changed nothing). Every workspace given is put through every output: check, export, the scene,
# each question of query (those about one entity, over a spread of identities), and every diagram
# in every format.
#
#   tools/same.sh <ds-before> <ds-after> [workspace ...]      (default: design and every example)
#
# Exits 0 when nothing differs, 1 with the list of what does.
set -u
before=$1
after=$2
shift 2
workspaces=("$@")
if [ ${#workspaces[@]} -eq 0 ]; then
  workspaces=(design)
  for d in examples/*/ tests/corpus/*/; do workspaces+=("${d%/}"); done
fi
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
differ=0
compared=0
same() { # label, then the arguments both builds are run with
  local label=$1
  shift
  "$before" "$@" >"$out/a" 2>"$out/a.err"
  local ca=$?
  "$after" "$@" >"$out/b" 2>"$out/b.err"
  local cb=$?
  compared=$((compared + 1))
  if [ $ca -ne $cb ] || ! cmp -s "$out/a" "$out/b" || ! cmp -s "$out/a.err" "$out/b.err"; then
    echo "DIFFERS: $label"
    differ=$((differ + 1))
  fi
}
for ws in "${workspaces[@]}"; do
  [ -d "$ws" ] || continue
  same "$ws check" check "$ws"
  same "$ws check --json" check "$ws" --json
  same "$ws export" export "$ws"
  same "$ws scene" render "$ws" -f json
  same "$ws page" render "$ws" -f html
  for question in diagrams entities findings coupling; do
    same "$ws query $question" query "$ws" "$question"
  done
  # the questions about one entity, over a spread of identities of every kind
  for id in $("$before" export "$ws" 2>/dev/null | grep -o '"id": *"[^"]*"' | sed 's/.*: *"//; s/"$//' | awk 'NR % 7 == 1' | head -120); do
    for question in relations artefacts behaviours entities; do
      same "$ws query $question $id" query "$ws" "$question" "$id"
    done
  done
  for diagram in $("$before" query "$ws" diagrams 2>/dev/null | awk '{print $2}' | sed 's/^diagram://'); do
    same "$ws shows $diagram" query "$ws" shows "$diagram"
    for format in ir svg text cells layout; do
      same "$ws $diagram -f $format" render "$ws" "$diagram" -f "$format"
    done
  done
done
echo "$compared outputs compared, $differ differ"
[ $differ -eq 0 ]
