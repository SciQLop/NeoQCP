#!/bin/bash
# Assemble the GitHub Pages site from a wasm build of the gallery.
# Usage: examples/wasm-gallery/build_site.sh <wasm-builddir> <outdir>
set -euo pipefail

builddir="$1"
outdir="$2"
here="$(dirname "$0")"

coi_version=0.1.7
coi_sha256=03d2f35be3249f5eeb4e214c5daf2be8067e2b24630aaef26a1247b723f2d05d

rm -rf "$outdir"
mkdir -p "$outdir"
cp "$here/index.html" "$outdir/"
cp "$builddir"/neoqcp_gallery.{js,wasm} "$builddir/qtloader.js" "$outdir/"

curl -sfL "https://cdn.jsdelivr.net/npm/coi-serviceworker@${coi_version}/coi-serviceworker.min.js" \
    -o "$outdir/coi-serviceworker.js"
echo "${coi_sha256}  $outdir/coi-serviceworker.js" | sha256sum -c --quiet

echo "Site assembled in $outdir/"
