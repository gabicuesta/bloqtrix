// Genera la portada para itch.io (630x500): docs/cover.png
// Uso: node tools/make_cover.js <48.rom> <captura.png de 512x384 sin filtros PNG (las del banco de pruebas)>
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const [romPath, shotPath] = process.argv.slice(2);
const rom = fs.readFileSync(romPath);
const W = 630, H = 500;
const img = Buffer.alloc(W * H * 3);   // negro
const px = (x, y, c) => { if (x < 0 || y < 0 || x >= W || y >= H) return; const o = (y * W + x) * 3; img[o] = c[0]; img[o + 1] = c[1]; img[o + 2] = c[2]; };
const rect = (x, y, w, h, c) => { for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) px(x + i, y + j, c); };
const C = { white: [255, 255, 255], yellow: [255, 255, 0], cyan: [0, 255, 255], blue: [0, 0, 255], grey: [205, 205, 205],
  red: [255, 0, 0], green: [0, 255, 0], magenta: [255, 0, 255], dark: [20, 20, 40] };

// franjas de Sinclair: cuatro bandas inclinadas en la esquina inferior derecha
const stripes = [C.red, C.yellow, C.green, C.cyan];
for (let y = H - 110; y < H; y++)
  stripes.forEach((col, i) => { const x0 = 470 + i * 22 + (H - y); for (let x = x0; x < x0 + 22; x++) px(x, y, col); });

// logotipo de bloques (letras de 3x5)
const LETTERS = {
  B: ['##.', '#.#', '##.', '#.#', '##.'], L: ['#..', '#..', '#..', '#..', '###'],
  O: ['###', '#.#', '#.#', '#.#', '###'], Q: ['###', '#.#', '#.#', '###', '..#'],
  T: ['###', '.#.', '.#.', '.#.', '.#.'], R: ['##.', '#.#', '##.', '#.#', '#.#'],
  I: ['###', '.#.', '.#.', '.#.', '###'], X: ['#.#', '#.#', '.#.', '#.#', '#.#'],
};
const CELL = 18, logoW = 31 * CELL, lx = (W - logoW) >> 1, ly = 22;
'BLOQTRIX'.split('').forEach((ch, i) => LETTERS[ch].forEach((r, y) => [...r].forEach((c, x) => {
  if (c !== '#') return;
  const X = lx + (i * 4 + x) * CELL, Y = ly + y * CELL, col = i < 4 ? C.white : C.yellow;
  rect(X, Y, CELL - 2, CELL - 2, col);
  rect(X + CELL - 2, Y, 2, CELL, [0, 0, 0]); rect(X, Y + CELL - 2, CELL, 2, [0, 0, 0]);
})));

// captura de la partida (PNG del banco de pruebas: filtro 0 en todas las filas)
{
  const b = fs.readFileSync(shotPath);
  let o = 8, idat = [], sw = 0, sh = 0;
  while (o < b.length) {
    const len = b.readUInt32BE(o), type = b.toString('ascii', o + 4, o + 8);
    if (type === 'IHDR') { sw = b.readUInt32BE(o + 8); sh = b.readUInt32BE(o + 12); }
    if (type === 'IDAT') idat.push(b.subarray(o + 8, o + 8 + len));
    o += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const scale = 0.75, dw = Math.round(sw * scale), dh = Math.round(sh * scale);
  const dx = (W - dw) >> 1, dy = ly + 5 * CELL + 18;
  rect(dx - 4, dy - 4, dw + 8, dh + 8, C.blue);
  for (let y = 0; y < dh; y++) for (let x = 0; x < dw; x++) {
    const sx = Math.floor(x / scale), sy = Math.floor(y / scale), so = sy * (sw * 3 + 1) + 1 + sx * 3;
    px(dx + x, dy + y, [raw[so], raw[so + 1], raw[so + 2]]);
  }
}

// texto con la fuente del ROM en negrita, a escala
function text(s, y, scale, col) {
  const x0 = 24;
  [...s].forEach((ch, i) => {
    let g;
    if (ch === '©') g = [0x3C, 0x42, 0x99, 0xA1, 0xA1, 0x99, 0x42, 0x3C];
    else { const off = 0x3D00 + (ch.charCodeAt(0) - 32) * 8; g = [...rom.subarray(off, off + 8)].map(v => v | (v >> 1)); }
    g.forEach((row, gy) => { for (let gx = 0; gx < 8; gx++) if (row & (0x80 >> gx)) rect(x0 + (i * 8 + gx) * scale, y + gy * scale, scale, scale, col); });
  });
}
text('ZX SPECTRUM 48K / 128K', 446, 2, C.cyan);
text('© 2026 GABRIEL CUESTA', 474, 2, C.grey);

// PNG
const rawOut = Buffer.alloc((W * 3 + 1) * H);
for (let y = 0; y < H; y++) img.copy(rawOut, y * (W * 3 + 1) + 1, y * W * 3, (y + 1) * W * 3);
const crcT = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = b => { let c = 0xFFFFFFFF; for (const x of b) c = crcT[(c ^ x) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(H, 4); ih[8] = 8; ih[9] = 2;
const out = path.join(__dirname, '..', 'docs', 'cover.png');
fs.writeFileSync(out, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(rawOut)), chunk('IEND', Buffer.alloc(0))]));
console.log('docs/cover.png escrito');
