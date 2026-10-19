// BLOQTRIX ZX — gráficos.
// La pantalla del Spectrum: 256x192 píxeles en 0x4000 (6144 bytes, en el orden entrelazado
// del ULA) y 32x24 atributos de color en 0x5800 (un byte por cada bloque de 8x8 píxeles).
// Cada casilla del tablero ocupa 2x2 bloques con un único atributo, así que nunca hay
// "choque de color" dentro de una casilla.
#include <string.h>
#include "gfx.h"
#include "engine.h"

#define SCREEN ((uint8_t *)0x4000)
#define ATTRS  ((uint8_t *)0x5800)
#define ROM_FONT ((const uint8_t *)0x3D00)

// Índices = colores lógicos del motor (C_EMPTY, C_YELLOW, ...)
const uint8_t color_paper_attr[] = {
  INK(Z_BLUE),
  A_BRIGHT | PAPER(Z_YELLOW), A_BRIGHT | PAPER(Z_CYAN), A_BRIGHT | PAPER(Z_RED), A_BRIGHT | PAPER(Z_GREEN),
  A_BRIGHT | PAPER(Z_MAGENTA), PAPER(Z_MAGENTA), A_BRIGHT | PAPER(Z_BLUE), PAPER(Z_YELLOW),
  PAPER(Z_CYAN), PAPER(Z_GREEN), A_BRIGHT | PAPER(Z_WHITE), PAPER(Z_WHITE), PAPER(Z_WHITE),
};
const uint8_t color_ink_attr[] = {
  INK(Z_BLUE),
  A_BRIGHT | INK(Z_YELLOW), A_BRIGHT | INK(Z_CYAN), A_BRIGHT | INK(Z_RED), A_BRIGHT | INK(Z_GREEN),
  A_BRIGHT | INK(Z_MAGENTA), INK(Z_MAGENTA), A_BRIGHT | INK(Z_BLUE), INK(Z_YELLOW),
  INK(Z_CYAN), INK(Z_GREEN), A_BRIGHT | INK(Z_WHITE), INK(Z_WHITE), INK(Z_WHITE),
};

// Fuente: la del ROM en negrita (cada fila OR consigo misma desplazada) + 8 letras propias.
static uint8_t font[(96 + 8) * 8];

uint8_t bmp_block[32], bmp_empty[32], bmp_ghost[32], bmp_stone[32], bmp_cracked[32];
uint8_t bmp_bomb[2][32];
uint8_t bmp8_block[8], bmp8_ghost[8], bmp8_bomb[8], bmp8_stone[8];

// ---------- Dibujos en "pixel art" de texto: '#' = tinta, '.' = papel ----------
static void art16(uint8_t *out, const char *const *rows, uint8_t nrows) {
  uint8_t y, x, v;
  for (y = 0; y < nrows; ++y)
    for (x = 0; x < 16; ++x) {
      v = 0x80 >> (x & 7);
      if (rows[y][x] == '#') out[y * 2 + (x >> 3)] |= v;
    }
}
static void art8(uint8_t *out, const char *const *rows, uint8_t nrows) {
  uint8_t y, x;
  for (y = 0; y < nrows; ++y)
    for (x = 0; x < 8; ++x)
      if (rows[y][x] == '#') out[y] |= 0x80 >> x;
}
static void set16(uint8_t *b, uint8_t x, uint8_t y) { b[y * 2 + (x >> 3)] |= 0x80 >> (x & 7); }

// Casilla llena: 14x14 de color con sombra oscura abajo y a la derecha y 1 px de separación.
static const char *const ART_BLOCK[16] = {
  "..............##", "..............##", "..............##", "..............##",
  "..............##", "..............##", "..............##", "..............##",
  "..............##", "..............##", "..............##", "..............##",
  "..............##", "..............##", ".###############", "################",
};
// Casilla vacía: contorno fino (en azul oscuro sobre negro)
static const char *const ART_EMPTY[16] = {
  "###############.", "#.............#.", "#.............#.", "#.............#.",
  "#.............#.", "#.............#.", "#.............#.", "#.............#.",
  "#.............#.", "#.............#.", "#.............#.", "#.............#.",
  "#.............#.", "#.............#.", "###############.", "................",
};
// Bomba (encima del bloque): cuerpo, brillo, tapa y mecha; la chispa va en dos fotogramas.
static const char *const ART_BOMB[16] = {
  "................", "................", "................", ".........#......",
  "........#.......", ".....####.......", "...########.....", "..##..######....",
  ".##.#########...", ".##.#########...", ".############...", ".############...",
  "..##########....", "...########.....", "................", "................",
};
static const char *const ART_SPARK0[3] = { "..........#.#...", "...........#....", "..........#.#..." };
static const char *const ART_SPARK1[3] = { "...........#....", "..........###...", "...........#...." };

static const char *const ART8_BLOCK[8] = {
  "......##", "......##", "......##", "......##", "......##", "......##", ".#######", "########",
};
static const char *const ART8_GHOST[8] = {
  "#.#.#.#.", ".#.#.#..", "#.#.#.#.", ".#.#.#..", "#.#.#.#.", ".#.#.#..", "#.#.#.#.", "........",
};
static const char *const ART8_BOMB[6] = {
  ".....#..", "....#...", "..###...", ".#.###..", ".#####..", "..###...",
};

static void make_font(void) {
  uint16_t i;
  uint8_t b;
  for (i = 0; i < 96 * 8; ++i) { b = ROM_FONT[i]; font[i] = b | (b >> 1); }
  // É Í Ó Á Ú Ñ: la letra del ROM con la tilde en la fila 0
  {
    static const char base[6] = { 'E', 'I', 'O', 'A', 'U', 'N' };
    uint8_t k, j;
    for (k = 0; k < 6; ++k) {
      for (j = 0; j < 8; ++j) font[(96 + k) * 8 + j] = font[(base[k] - 32) * 8 + j];
      font[(96 + k) * 8] = k == 5 ? 0x3C : 0x0C;
    }
  }
  // ¡ y ¿: el ! y el ? del ROM dados la vuelta
  {
    uint8_t j, v, m, x;
    for (j = 0; j < 8; ++j) font[(96 + 6) * 8 + j] = font[('!' - 32) * 8 + 7 - j];
    for (j = 0; j < 8; ++j) {
      v = font[('?' - 32) * 8 + 7 - j]; m = 0;
      for (x = 0; x < 8; ++x) if (v & (1 << x)) m |= 0x80 >> x;
      font[(96 + 7) * 8 + j] = m;
    }
  }
}

void gfx_init(void) {
  uint8_t y, x, j;
  make_font();

  art16(bmp_block, ART_BLOCK, 16);
  art16(bmp_empty, ART_EMPTY, 16);

  // sombra de la pieza: contorno + trama de puntos
  art16(bmp_ghost, ART_EMPTY, 16);
  for (y = 1; y < 14; ++y)
    for (x = 1; x < 14; ++x)
      if (((x + y) & 1) == 0) set16(bmp_ghost, x, y);

  // piedra: bloque con rayado diagonal
  memcpy(bmp_stone, bmp_block, 32);
  for (y = 0; y < 14; ++y)
    for (x = 0; x < 14; ++x)
      if (((x - y) & 3) == 0) set16(bmp_stone, x, y);

  // piedra agrietada: sin rayado y con una grieta gruesa en zigzag de arriba abajo
  {
    static const uint8_t crack[][2] = {
      {6,0},{6,1},{5,2},{5,3},{6,4},{7,5},{8,6},{8,7},{7,8},{6,9},{6,10},{7,11},{8,12},{8,13},
      {9,6},{10,7},{11,8},{12,8},{5,9},{4,10},{3,10},{2,11},
    };
    memcpy(bmp_cracked, bmp_block, 32);
    for (j = 0; j < sizeof crack / 2; ++j) {
      x = crack[j][0]; y = crack[j][1];
      set16(bmp_cracked, x, y);
      set16(bmp_cracked, x + 1, y);
    }
    set16(bmp_cracked, 1, 1); set16(bmp_cracked, 11, 2); set16(bmp_cracked, 3, 12);   // esquirlas
  }

  // bomba: bloque + sprite + chispa (dos fotogramas)
  for (j = 0; j < 2; ++j) {
    memcpy(bmp_bomb[j], bmp_block, 32);
    art16(bmp_bomb[j], ART_BOMB, 16);
    art16(bmp_bomb[j], j ? ART_SPARK1 : ART_SPARK0, 3);
  }

  art8(bmp8_block, ART8_BLOCK, 8);
  art8(bmp8_ghost, ART8_GHOST, 8);
  memcpy(bmp8_bomb, bmp8_block, 8); art8(bmp8_bomb, ART8_BOMB, 6);
  memcpy(bmp8_stone, bmp8_block, 8);
  for (y = 0; y < 6; ++y)
    for (x = 0; x < 6; ++x)
      if (((x + 6 - y) % 3) == 0) bmp8_stone[y] |= 0x80 >> x;
}

void gfx_cls(void) {
  memset(SCREEN, 0, 6144);
  memset(ATTRS, INK(Z_WHITE), 768);
}

uint8_t *scr_addr(uint8_t row, uint8_t col) {
  return SCREEN + ((uint16_t)(row & 0x18) << 8) + ((row & 7) << 5) + col;
}

void set_attr(uint8_t row, uint8_t col, uint8_t a) { ATTRS[(uint16_t)row * 32 + col] = a; }

void fill_attr(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t a) {
  uint8_t *p = ATTRS + (uint16_t)row * 32 + col;
  for (; h; --h, p += 32) memset(p, a, w);
}

void clear_area(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t a) {
  uint8_t r, y;
  uint8_t *p;
  for (r = row; r < row + h; ++r) {
    p = scr_addr(r, col);
    for (y = 0; y < 8; ++y, p += 256) memset(p, 0, w);
  }
  fill_attr(row, col, w, h, a);
}

void draw8(uint8_t row, uint8_t col, const uint8_t *bmp, uint8_t a) {
  uint8_t *p = scr_addr(row, col);
  uint8_t y;
  for (y = 0; y < 8; ++y, p += 256) *p = bmp[y];
  ATTRS[(uint16_t)row * 32 + col] = a;
}

void draw16(uint8_t row, uint8_t col, const uint8_t *bmp, uint8_t a) {
  uint8_t *p = scr_addr(row, col);
  uint8_t *q;
  uint8_t y;
  for (y = 0; y < 8; ++y, p += 256, bmp += 2) { p[0] = bmp[0]; p[1] = bmp[1]; }
  p = scr_addr(row + 1, col);
  for (y = 0; y < 8; ++y, p += 256, bmp += 2) { p[0] = bmp[0]; p[1] = bmp[1]; }
  q = ATTRS + (uint16_t)row * 32 + col;
  q[0] = a; q[1] = a; q[32] = a; q[33] = a;
}

void attr16(uint8_t row, uint8_t col, uint8_t a) {
  uint8_t *q = ATTRS + (uint16_t)row * 32 + col;
  q[0] = a; q[1] = a; q[32] = a; q[33] = a;
}

static const uint8_t *glyph(uint8_t ch) {
  if (ch >= 128 && ch < 136) return font + (96 + ch - 128) * 8;
  if (ch < 32 || ch > 127) ch = '?';
  return font + (ch - 32) * 8;
}

uint8_t text_len(const char *s) { uint8_t n = 0; while (*s++) ++n; return n; }

void print_at(uint8_t row, uint8_t col, const char *s, uint8_t a) {
  for (; *s && col < 32; ++s, ++col) draw8(row, col, glyph((uint8_t)*s), a);
}

void print_tall(uint8_t row, uint8_t col, const char *s, uint8_t a) {
  const uint8_t *g;
  uint8_t *p;
  uint8_t y;
  for (; *s && col < 32; ++s, ++col) {
    g = glyph((uint8_t)*s);
    p = scr_addr(row, col);
    for (y = 0; y < 4; ++y) { *p = g[y]; p += 256; *p = g[y]; p += 256; }
    p = scr_addr(row + 1, col);
    for (y = 4; y < 8; ++y) { *p = g[y]; p += 256; *p = g[y]; p += 256; }
    set_attr(row, col, a); set_attr(row + 1, col, a);
  }
}

// Cada píxel de la letra se convierte en 2x2: cada carácter ocupa 2x2 bloques.
static uint8_t widen(uint8_t nib) {
  uint8_t r = 0, i;
  for (i = 0; i < 4; ++i) if (nib & (8 >> i)) r |= 0xC0 >> (i * 2);
  return r;
}
void print_huge(uint8_t row, uint8_t col, const char *s, uint8_t a) {
  const uint8_t *g;
  uint8_t *p;
  uint8_t y, l, r;
  for (; *s && col < 31; ++s, col += 2) {
    g = glyph((uint8_t)*s);
    p = scr_addr(row, col);
    for (y = 0; y < 4; ++y) {
      l = widen(g[y] >> 4); r = widen(g[y] & 15);
      p[0] = l; p[1] = r; p += 256; p[0] = l; p[1] = r; p += 256;
    }
    p = scr_addr(row + 1, col);
    for (y = 4; y < 8; ++y) {
      l = widen(g[y] >> 4); r = widen(g[y] & 15);
      p[0] = l; p[1] = r; p += 256; p[0] = l; p[1] = r; p += 256;
    }
    fill_attr(row, col, 2, 2, a);
  }
}

// Marco de 1 píxel alrededor de un rectángulo de w x h bloques (pegado por fuera).
void draw_frame(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t a) {
  uint8_t i, y;
  uint8_t *p;
  // arriba y abajo
  for (i = 0; i < w; ++i) {
    p = scr_addr(row - 1, col + i) + 7 * 256; *p = 0xFF;
    p = scr_addr(row + h, col + i); *p = 0xFF;
  }
  // izquierda y derecha
  for (i = 0; i < h; ++i) {
    p = scr_addr(row + i, col - 1);
    for (y = 0; y < 8; ++y, p += 256) *p = 0x01;
    p = scr_addr(row + i, col + w);
    for (y = 0; y < 8; ++y, p += 256) *p = 0x80;
  }
  // esquinas
  *(scr_addr(row - 1, col - 1) + 7 * 256) = 0x01;
  *(scr_addr(row - 1, col + w) + 7 * 256) = 0x80;
  *scr_addr(row + h, col - 1) = 0x01;
  *scr_addr(row + h, col + w) = 0x80;
  fill_attr(row - 1, col - 1, w + 2, 1, a);
  fill_attr(row + h, col - 1, w + 2, 1, a);
  for (i = 0; i < h; ++i) { set_attr(row + i, col - 1, a); set_attr(row + i, col + w, a); }
}
