#!/usr/bin/env bash
# Compila BLOQTRIX para ZX Spectrum:
#   build/bloqtrix48.tap  / build/bloqtrix48.sna   (48K, beeper)
#   build/bloqtrix128.tap / build/bloqtrix128.sna  (128K, chip AY)
# Requiere z88dk (https://github.com/z88dk/z88dk) con zcc en el PATH y ZCCCFG configurado, y Node.js.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
node tools/gen_notes.js >/dev/null
COMMON="+zx -vn -startup=31 -clib=sdcc_iy -SO3 --max-allocs-per-node20000 -pragma-include:src/zpragma.inc"
SRC="src/main.c src/engine.c src/gfx.c"
zcc $COMMON -DMODEL48  $SRC src/sound48.c  -o build/bloqtrix48  -create-app -Cz--screen -Czassets/loading.scr
zcc $COMMON -DMODEL48  $SRC src/sound48.c  -o build/bloqtrix48  -create-app -subtype=sna -m
zcc $COMMON -DMODEL128 $SRC src/sound128.c -o build/bloqtrix128 -create-app -Cz--screen -Czassets/loading.scr
zcc $COMMON -DMODEL128 $SRC src/sound128.c -o build/bloqtrix128 -create-app -subtype=sna -Cz--128 -m
rm -f build/*.bin build/bloqtrix48 build/bloqtrix128
ls -l build/*.tap build/*.sna
