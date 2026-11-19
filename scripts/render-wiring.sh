#!/usr/bin/env sh
# Render the WireViz harness diagrams: docs/wiring-*.yml -> docs/images/wiring-*.svg
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
    for source in docs/wiring-*.yml; do
      wireviz --format s --output-dir docs/images "$source" >/dev/null
      echo "rendered docs/images/$(basename "$source" .yml).svg"
    done
    chown -R "$OWNER" docs/images
  '
