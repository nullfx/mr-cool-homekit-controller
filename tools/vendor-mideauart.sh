#!/usr/bin/env bash
# Re-vendor dudanov/MideaUART into lib/MideaUART in Arduino library layout.
#   tools/vendor-mideauart.sh [tag]     (default: v1.1.9)
set -euo pipefail

TAG="${1:-v1.1.9}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/lib/MideaUART"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

git clone --quiet --depth 1 --branch "$TAG" https://github.com/dudanov/MideaUART.git "$TMP/MideaUART"
COMMIT="$(git -C "$TMP/MideaUART" rev-parse HEAD)"

mv "$DEST/src/MideaUART.h" "$TMP/MideaUART.h"   # our Arduino entry header
rm -rf "$DEST/src"
mkdir -p "$DEST/src"
mv "$TMP/MideaUART.h" "$DEST/src/MideaUART.h"
cp -R "$TMP/MideaUART/include/." "$DEST/src/"
cp -R "$TMP/MideaUART/src/." "$DEST/src/"
cp "$TMP/MideaUART/README.md" "$DEST/UPSTREAM_README.md"

sed -i.bak "s/^version=.*/version=${TAG#v}/" "$DEST/library.properties" && rm "$DEST/library.properties.bak"
echo "Vendored MideaUART $TAG ($COMMIT) into $DEST"
echo "Update the tag/commit in $DEST/VENDORED.md."
