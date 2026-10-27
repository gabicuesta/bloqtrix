#!/usr/bin/env bash
# Prepara los paquetes de publicación en release/:
#   bloqtrix-zx-v1.0.zip       descargas: .tap y .sna de las dos variantes, instrucciones y licencia
#   bloqtrix-zx-web-v1.0.zip   versión jugable en el navegador para itch.io (emulador JSSpeccy 3)
# Uso: JSSPECCY=/ruta/a/jsspeccy-3.x tools/make_release.sh   (la carpeta que contiene jsspeccy/)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=v1.0
: "${JSSPECCY:?Indica en JSSPECCY la carpeta descomprimida de JSSpeccy 3 (la que contiene jsspeccy/)}"
ZIP() { if command -v zip >/dev/null; then (cd "$1" && zip -qr "$2" .); else (cd "$1" && /c/Windows/System32/tar.exe -a -c -f "$2" *); fi; }

rm -rf release/pkg release/web
mkdir -p release/pkg release/web

cp build/bloqtrix48.tap build/bloqtrix48.sna build/bloqtrix128.tap build/bloqtrix128.sna release/pkg/
cp INSTRUCCIONES.txt INSTRUCTIONS.txt release/pkg/
cp ../LICENSE release/pkg/LICENSE.txt
rm -f "release/bloqtrix-zx-$VERSION.zip"
ZIP release/pkg "../bloqtrix-zx-$VERSION.zip"

cp itch/index.html build/bloqtrix48.sna build/bloqtrix128.sna release/web/
mkdir -p release/web/jsspeccy
cp -r "$JSSPECCY/jsspeccy/jsspeccy.js" "$JSSPECCY/jsspeccy/jsspeccy-worker.js" "$JSSPECCY/jsspeccy/jsspeccy-core.wasm" \
      "$JSSPECCY/jsspeccy/roms" "$JSSPECCY/jsspeccy/tapeloaders" release/web/jsspeccy/
cp "$JSSPECCY/COPYING" release/web/jsspeccy/COPYING.txt
find release/web -name .DS_Store -delete
rm -f "release/bloqtrix-zx-web-$VERSION.zip"
ZIP release/web "../bloqtrix-zx-web-$VERSION.zip"

ls -l release/*.zip
