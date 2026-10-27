// Genera la pantalla de carga: assets/loading.scr (formato del Spectrum, 6912 bytes) y
// docs/loading.png (vista previa). Uso: node tools/make_loading.js <ruta al ROM 48K>
// El ROM solo se usa para la fuente del texto (la misma que usa el juego en tiempo de ejecución).
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const romPath = process.argv[2];
if (!romPath) { console.error('uso: node tools/make_loading.js <48.rom>'); process.exit(1); }
const rom = fs.readFileSync(romPath);

const pix = new Uint8Array(256 * 192);   // 1 = tinta
const attr = new Uint8Array(32 * 24);    // atributos por bloque de 8x8
const BRIGHT = 0x40, FLASH = 0x80;
const PAPER = c => c << 3, INK = c => c;
const [BLACK, BLUE, RED, MAGENTA, GREEN, CYAN, YELLOW, WHITE] = [0, 1, 2, 3, 4, 5, 6, 7];

const setPix = (x, y) => { if (x >= 0 && x < 256 && y >= 0 && y < 192) pix[y * 256 + x] = 1; };
const art = rows => rows.map(r => [...r].map(ch => ch === '#'));
const draw = (bits, x0, y0) => bits.forEach((row, y) => row.forEach((on, x) => { if (on) setPix(x0 + x, y0 + y); }));
const setAttr = (row, col, a) => { attr[row * 32 + col] = a; };

// --- dibujos (los mismos que src/gfx.c) ---
const BLOCK16 = art([
  '..............##', '..............##', '..............##', '..............##',
  '..............##', '..............##', '..............##', '..............##',
  '..............##', '..............##', '..............##', '..............##',
  '..............##', '..............##', '.###############', '################',
]);
const EMPTY16 = art([
  '###############.', '#.............#.', '#.............#.', '#.............#.',
  '#.............#.', '#.............#.', '#.............#.', '#.............#.',
  '#.............#.', '#.............#.', '#.............#.', '#.............#.',
  '#.............#.', '#.............#.', '###############.', '................',
]);
const BOMB16 = art([
  '..........#.#...', '...........#....', '..........#.#...', '.........#......',
  '........#.......', '.....####.......', '...########.....', '..##..######....',
  '.##.#########...', '.##.#########...', '.############...', '.############...',
  '..##########....', '...########.....', '................', '................',
]);
// bloque del logotipo: casi macizo, con una línea de separación abajo y a la derecha
const BLOCK8 = art(['.......#', '.......#', '.......#', '.......#', '.......#', '.......#', '.......#', '########']);

const ghost16 = EMPTY16.map((row, y) => row.map((on, x) => on || (x > 0 && x < 14 && y > 0 && y < 14 && ((x + y) & 1) === 0)));
const stone16 = BLOCK16.map((row, y) => row.map((on, x) => on || (x < 14 && y < 14 && (((x - y) % 4) + 4) % 4 === 0)));

function cell16(row, col, bits, a) {
  draw(bits, col * 8, row * 8);
  setAttr(row, col, a); setAttr(row, col + 1, a); setAttr(row + 1, col, a); setAttr(row + 1, col + 1, a);
}

// --- logotipo con bloques de 8x8: letras de 3x5 ---
const LETTERS = {
  B: ['##.', '#.#', '##.', '#.#', '##.'], L: ['#..', '#..', '#..', '#..', '###'],
  O: ['###', '#.#', '#.#', '#.#', '###'], Q: ['###', '#.#', '#.#', '###', '..#'],
  T: ['###', '.#.', '.#.', '.#.', '.#.'], R: ['##.', '#.#', '##.', '#.#', '#.#'],
  I: ['###', '.#.', '.#.', '.#.', '###'], X: ['#.#', '#.#', '.#.', '#.#', '#.#'],
};
const LOGO_ROW = 2;
'BLOQTRIX'.split('').forEach((ch, i) => {
  const color = i < 4 ? BRIGHT | PAPER(WHITE) : BRIGHT | PAPER(YELLOW);
  LETTERS[ch].forEach((r, y) => [...r].forEach((c, x) => {
    if (c !== '#') return;
    const col = i * 4 + x, row = LOGO_ROW + y;
    draw(BLOCK8, col * 8, row * 8);
    setAttr(row, col, color);
  }));
});

// --- tablero de muestra: 8 x 6 casillas, columnas 8..23, filas 9..20 ---
const BR = 9, BC = 8;
const P = c => BRIGHT | PAPER(c);
const board = [
  //    0          1          2          3          4          5          6          7
  [null,      null,      null,      'ghost',   'ghost',   'ghost',   null,      null],
  [null,      null,      null,      null,      'ghost',   null,      null,      null],
  [null,      P(CYAN),   null,      null,      null,      null,      P(GREEN),  null],
  [P(MAGENTA),P(CYAN),   P(CYAN),   null,      null,      PAPER(WHITE), P(GREEN), P(GREEN)],
  [P(MAGENTA),'stone',   P(YELLOW), P(YELLOW), null,      PAPER(WHITE), 'bomb',  P(GREEN)],
  [P(MAGENTA),P(MAGENTA),P(YELLOW), P(YELLOW), null,      P(RED),    P(RED),    P(BLUE)],
];
board.forEach((rowCells, r) => rowCells.forEach((v, c) => {
  const row = BR + r * 2, col = BC + c * 2;
  if (v === null) cell16(row, col, EMPTY16, INK(BLUE));
  else if (v === 'ghost') cell16(row, col, ghost16, BRIGHT | INK(MAGENTA));
  else if (v === 'stone') cell16(row, col, stone16, PAPER(WHITE));
  else if (v === 'bomb') cell16(row, col, BLOCK16.map((row2, y) => row2.map((on, x) => on || BOMB16[y][x])), P(BLUE));
  else cell16(row, col, BLOCK16, v);
}));
// la última fila está a una casilla de completarse: parpadea (atributo FLASH) mientras carga
for (let c = 0; c < 8; c++) if (board[5][c] !== null) {
  const row = BR + 10, col = BC + c * 2;
  for (const [dr, dc] of [[0, 0], [0, 1], [1, 0], [1, 1]]) attr[(row + dr) * 32 + col + dc] |= FLASH;
}
// marco del tablero
for (let x = BC * 8 - 1; x <= (BC + 16) * 8; x++) { setPix(x, BR * 8 - 1); setPix(x, (BR + 12) * 8); }
for (let y = BR * 8 - 1; y <= (BR + 12) * 8; y++) { setPix(BC * 8 - 1, y); setPix((BC + 16) * 8, y); }
for (let r = BR - 1; r <= BR + 12; r++) { if (!attr[r * 32 + BC - 1]) setAttr(r, BC - 1, BRIGHT | INK(BLUE)); if (!attr[r * 32 + BC + 16]) setAttr(r, BC + 16, BRIGHT | INK(BLUE)); }
for (let c = BC - 1; c <= BC + 16; c++) { if (!attr[(BR - 1) * 32 + c]) setAttr(BR - 1, c, BRIGHT | INK(BLUE)); if (!attr[(BR + 12) * 32 + c]) setAttr(BR + 12, c, BRIGHT | INK(BLUE)); }

// --- texto con la fuente del ROM en negrita ---
function text(row, s, a) {
  const col0 = (32 - s.length) >> 1;
  [...s].forEach((ch, i) => {
    let g;
    if (ch === '©') g = [0x3C, 0x42, 0x99, 0xA1, 0xA1, 0x99, 0x42, 0x3C];
    else { const o = 0x3D00 + (ch.charCodeAt(0) - 32) * 8; g = [...rom.subarray(o, o + 8)].map(b => b | (b >> 1)); }
    g.forEach((b, y) => { for (let x = 0; x < 8; x++) if (b & (0x80 >> x)) setPix((col0 + i) * 8 + x, row * 8 + y); });
    setAttr(row, col0 + i, a);
  });
}
text(22, 'ZX SPECTRUM 48K / 128K', BRIGHT | INK(CYAN));
text(23, '© 2026 GABRIEL CUESTA', INK(WHITE));

// --- volcado a .scr (orden entrelazado del ULA) ---
const scr = Buffer.alloc(6912);
for (let y = 0; y < 192; y++)
  for (let xb = 0; xb < 32; xb++) {
    let v = 0;
    for (let b = 0; b < 8; b++) if (pix[y * 256 + xb * 8 + b]) v |= 0x80 >> b;
    const addr = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2) | xb;
    scr[addr] = v;
  }
for (let i = 0; i < 768; i++) scr[6144 + i] = attr[i] || INK(WHITE);
const root = path.join(__dirname, '..');
fs.mkdirSync(path.join(root, 'assets'), { recursive: true });
fs.writeFileSync(path.join(root, 'assets', 'loading.scr'), scr);

// --- vista previa PNG (2x) ---
const pal = [[0,0,0],[0,0,205],[205,0,0],[205,0,205],[0,205,0],[0,205,205],[205,205,0],[205,205,205],
             [0,0,0],[0,0,255],[255,0,0],[255,0,255],[0,255,0],[0,255,255],[255,255,0],[255,255,255]];
const W = 512, H = 384, raw = Buffer.alloc((W * 3 + 1) * H);
for (let y = 0; y < 192; y++) for (let x = 0; x < 256; x++) {
  const a = scr[6144 + (y >> 3) * 32 + (x >> 3)];
  const on = pix[y * 256 + x], br = (a & 0x40) ? 8 : 0;
  const c = pal[(on ? (a & 7) : ((a >> 3) & 7)) + br];
  for (let dy = 0; dy < 2; dy++) for (let dx = 0; dx < 2; dx++) {
    const o = (y * 2 + dy) * (W * 3 + 1) + 1 + (x * 2 + dx) * 3;
    raw[o] = c[0]; raw[o + 1] = c[1]; raw[o + 2] = c[2];
  }
}
const crcT = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = b => { let c = 0xFFFFFFFF; for (const x of b) c = crcT[(c ^ x) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(H, 4); ih[8] = 8; ih[9] = 2;
fs.mkdirSync(path.join(root, 'docs'), { recursive: true });
fs.writeFileSync(path.join(root, 'docs', 'loading.png'), Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
console.log('assets/loading.scr y docs/loading.png escritos');
