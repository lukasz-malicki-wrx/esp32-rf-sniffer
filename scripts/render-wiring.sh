#!/usr/bin/env sh
# Render docs/wiring.yml -> docs/images/wiring.{svg,png} + docs/wiring.bom.tsv.
#
# WireViz and Graphviz run in a throwaway container: nothing to install locally, and the
# output comes from the same pinned tool versions on every machine.
set -eu

cd "$(dirname "$0")/.."

docker run --rm \
  -v "$PWD:/work" -w /work \
  -e OWNER="$(id -u):$(id -g)" \
  python:3.12-slim-bookworm \
  sh -ec '
    apt-get update -qq >/dev/null
    apt-get install -y -qq --no-install-recommends graphviz >/dev/null 2>&1
    pip install -q --disable-pip-version-check --root-user-action=ignore wireviz==0.4.1
    wireviz --format sp --output-dir docs/images docs/wiring.yml
    wireviz --format t --output-dir docs docs/wiring.yml
    chown -R "$OWNER" docs
  '

echo "Rendered docs/images/wiring.svg, docs/images/wiring.png and docs/wiring.bom.tsv"
