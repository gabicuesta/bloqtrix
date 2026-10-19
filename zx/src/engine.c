// BLOQTRIX ZX — motor del juego. Misma lógica que la versión web (index.html),
// adaptada a un Z80 a 3,5 MHz: el tablero se guarda como 8 bytes (uno por fila)
// para que comprobar si una pieza cabe sean unas pocas operaciones AND.
#include <string.h>
#include "engine.h"

uint8_t occ[N];
uint8_t cellc[N * N];
uint8_t cellk[N * N];
ClearResult clr;

// ---------- Aleatorio ----------
// xorshift de 16 bits (hl ^= hl<<7; hl ^= hl>>9; hl ^= hl<<8) en la versión en
// ensamblador de John Metcalf: unos 100 ciclos frente a los 400 del mismo cálculo en C.
uint16_t rng = 0xACE1;
void rng_seed(uint16_t s) { rng = s ? s : 0xACE1; }
uint8_t rnd8(void) __naked {
  __asm
    ld hl,(_rng)
    ld a,h
    rra
    ld a,l
    rra
    xor h
    ld h,a
    ld a,l
    rra
    ld a,h
    rra
    xor l
    ld l,a
    xor h
    ld h,a
    ld (_rng),hl
    ret
  __endasm;
}
// Número entre 0 y n-1: (aleatorio * n) / 256, con una multiplicación de 8x8 bits a mano.
uint8_t rnd_below(uint8_t n) __z88dk_fastcall __naked {
  (void)n;
  __asm
    ld e,l
    call _rnd8
    ld a,l
    ld hl,0
    ld d,h
    ld b,8
  rb_loop:
    add hl,hl
    rla
    jr nc,rb_skip
    add hl,de
  rb_skip:
    djnz rb_loop
    ld l,h
    ret
  __endasm;
}

// Tabla de "dilatación": dil[m][v] = OR de (v >> x) para cada bit x de la máscara m (x = 0..2).
// Con ella, saber en qué columnas choca una fila de pieza contra una fila del tablero es
// una sola consulta, en vez de desplazamientos variables (que el Z80 hace bit a bit).
// Vive en una dirección fija alineada a 256 bytes (DIL_ADDR, ver zpragma.inc) para que
// el ensamblador la indexe con el byte alto = DIL_HI + máscara y el byte bajo = fila.
#ifndef DIL_ADDR
#define DIL_ADDR 0xF400
#endif
#define DIL ((uint8_t *)DIL_ADDR)

// shl[c * 8 + m] = m << c (filas de pieza de hasta 3 bits desplazadas hasta 7 columnas)
static uint8_t shl[64];

void engine_init(void) {
  uint8_t m, c;
  uint16_t v;
  for (m = 0; m < 8; ++m)
    for (v = 0; v < 256; ++v)
      DIL[((uint16_t)m << 8) | v] = ((m & 1) ? v : 0) | ((m & 2) ? (v >> 1) : 0) | ((m & 4) ? (v >> 2) : 0);
  for (c = 0; c < 8; ++c)
    for (m = 0; m < 8; ++m) shl[c * 8 + m] = m << c;
}

// ---------- Catálogo de piezas ----------
// Cada forma: alto, ancho y máscara de cada fila ('#..' -> bit 0).
typedef struct { uint8_t h, w, r0, r1, r2; } Shape;
static const Shape shapes[] = {
  {3,3, 7,7,7},                                         //  0 cuadrado 3x3
  {2,2, 3,3,0},                                         //  1 cuadrado 2x2
  {2,3, 3,6,0},                                         //  2 Z izquierda
  {2,3, 6,3,0},                                         //  3 Z derecha
  {3,2, 1,1,3}, {2,3, 7,1,0}, {3,2, 3,2,2}, {2,3, 4,7,0},  //  4 L
  {3,2, 2,2,3}, {2,3, 1,7,0}, {3,2, 3,1,1}, {2,3, 7,4,0},  //  8 L invertida
  {2,3, 7,2,0}, {3,2, 1,3,1}, {2,3, 2,7,0}, {3,2, 2,3,2},  // 12 T
  {3,3, 1,1,7}, {3,3, 7,1,1}, {3,3, 7,4,4}, {3,3, 4,4,7},  // 16 esquina
  // piezas de ayuda (solo las usa el reparto con ayuda)
  {3,3, 1,2,4}, {3,3, 4,2,1},                           // 20 diagonal de 3
  {1,2, 3,0,0}, {2,1, 1,1,0},                           // 22 barra de 2
  {2,2, 1,2,0}, {2,2, 2,1,0},                           // 24 diagonal de 2
  {1,1, 1,0,0},                                         // 26 cuadrado suelto
};
#define NORMAL_SHAPES 20
#define HELPER_FIRST  20
#define HELPER_SHAPES 7

typedef struct { uint8_t color, weight, first, count; } Family;
static const Family fams[] = {
  {C_YELLOW, 8, 0, 1}, {C_CYAN, 12, 1, 1}, {C_RED, 10, 2, 1}, {C_GREEN, 10, 3, 1},
  {C_MAGENTA, 13, 4, 4}, {C_PINK, 13, 8, 4}, {C_BLUE, 12, 12, 4}, {C_ORANGE, 10, 16, 4},
  {C_LIME, 0, 20, 2}, {C_TEAL, 0, 22, 2}, {C_LIME, 0, 24, 2}, {C_WHITE, 0, 26, 1},
};
#define NORMAL_FAMS 8
#define NFAMS 12
#define TOTAL_WEIGHT 88

static uint8_t fam_of_shape(uint8_t sh) {
  uint8_t f;
  for (f = 0; f < NFAMS; ++f)
    if (sh >= fams[f].first && sh < fams[f].first + fams[f].count) return f;
  return 0;
}

static void make_piece(Piece *p, uint8_t sh) {
  const Shape *s = &shapes[sh];
  uint8_t y, x;
  p->fam = fam_of_shape(sh);
  p->color = fams[p->fam].color;
  p->h = s->h; p->w = s->w;
  p->rows[0] = s->r0; p->rows[1] = s->r1; p->rows[2] = s->r2;
  p->n = 0;
  for (y = 0; y < p->h; ++y)
    for (x = 0; x < p->w; ++x)
      if (p->rows[y] & (1 << x)) {
        p->cy[p->n] = y; p->cx[p->n] = x; p->kind[p->n] = K_NONE; ++p->n;
      }
}

static void random_normal(Piece *p) {
  uint8_t r = rnd_below(TOTAL_WEIGHT), f;
  for (f = 0; f < NORMAL_FAMS - 1; ++f) {
    if (r < fams[f].weight) break;
    r -= fams[f].weight;
  }
  make_piece(p, fams[f].first + rnd_below(fams[f].count));
}
static void random_helper(Piece *p) { make_piece(p, HELPER_FIRST + rnd_below(HELPER_SHAPES)); }

// ---------- Tablero ----------
uint8_t can_place(const uint8_t *b, const Piece *p, int8_t r, int8_t c) {
  uint8_t y;
  if (r < 0 || c < 0 || r + p->h > N || c + p->w > N) return 0;
  for (y = 0; y < p->h; ++y)
    if ((uint8_t)(p->rows[y] << c) & b[r + y]) return 0;
  return 1;
}

// Todas las posiciones válidas de una pieza de una vez: para cada fila de arranque r,
// vm[r] tiene un bit por cada columna donde cabe. Una columna c choca si alguna casilla
// (y, x) de la pieza cae sobre un bloque, es decir, si el bit c de (b[r+y] >> x) está a 1.
// Devuelve distinto de cero si hay al menos una.
// Las filas de pieza que no existen valen 0 y dil[0][x] = 0, así que se consultan siempre
// tres filas del tablero; leer hasta dos bytes más allá del final del tablero es inofensivo.
// En ensamblador: es la función más usada del reparto (unos 1.000 ciclos frente a 6.000 en C).
// Disposición de Piece: h en +2, w en +3, rows[] en +5.
static const uint8_t col_limit[4] = { 0, 0xFF, 0x7F, 0x3F };   // columnas de arranque posibles según el ancho
static uint8_t vp_any;
static uint8_t valid_positions(const uint8_t *b, const Piece *p, uint8_t *vm) __naked {
  (void)b; (void)p; (void)vm;
  __asm
    push ix
    ld hl,4
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)            ; de = b
    inc hl
    ld c,(hl)
    inc hl
    ld b,(hl)            ; bc = p
    inc hl
    ld a,(hl)
    inc hl
    ld h,(hl)
    ld l,a
    push hl
    pop ix               ; ix = vm
    xor a                ; vm[0..7] = 0
    ld (ix+0),a
    ld (ix+1),a
    ld (ix+2),a
    ld (ix+3),a
    ld (ix+4),a
    ld (ix+5),a
    ld (ix+6),a
    ld (ix+7),a
    ld (_vp_any),a
    ld h,b
    ld l,c
    inc hl
    inc hl
    ld a,(hl)            ; p->h
    neg
    add a,9
    ld b,a               ; filas de arranque: 9 - h
    inc hl
    ld a,(hl)            ; p->w
    push hl
    ld hl,_col_limit
    add a,l
    ld l,a
    ld a,h
    adc a,0
    ld h,a
    ld a,(hl)
    ld (vp_lim+1),a
    pop hl
    inc hl
    inc hl
    ld a,(hl)            ; rows[0]
    add a,DIL_ADDR / 256
    ld (vp_p0+1),a
    inc hl
    ld a,(hl)            ; rows[1]
    add a,DIL_ADDR / 256
    ld (vp_p1+1),a
    inc hl
    ld a,(hl)            ; rows[2]
    add a,DIL_ADDR / 256
    ld (vp_p2+1),a
  vp_row:
    ld a,(de)
    ld l,a
  vp_p0:
    ld h,0
    ld c,(hl)
    inc de
    ld a,(de)
    ld l,a
  vp_p1:
    ld h,0
    ld a,(hl)
    or c
    ld c,a
    inc de
    ld a,(de)
    ld l,a
  vp_p2:
    ld h,0
    ld a,(hl)
    or c
    cpl
  vp_lim:
    and 0
    ld (ix+0),a
    ld hl,_vp_any
    or (hl)
    ld (hl),a
    inc ix
    dec de
    djnz vp_row
    ld a,(_vp_any)
    ld l,a
    pop ix
    ret
  __endasm;
}

uint8_t fits_anywhere(const uint8_t *b, const Piece *p) {
  uint8_t vm[N];
  return valid_positions(b, p, vm) != 0;
}

uint8_t full_lines(const uint8_t *b, uint8_t *rows, uint8_t *cols) {
  uint8_t i, rm = 0, cm = 0xFF, n = 0;
  for (i = 0; i < N; ++i) {
    cm &= b[i];
    if (b[i] == 0xFF) { rm |= 1 << i; ++n; }
  }
  *rows = rm; *cols = cm;
  for (i = 0; i < N; ++i) if (cm & (1 << i)) ++n;
  return n;
}

void preview_lines(const Piece *p, uint8_t r, uint8_t c, uint8_t *rows, uint8_t *cols) {
  uint8_t b[N], y;
  memcpy(b, occ, N);
  for (y = 0; y < p->h; ++y) b[r + y] |= p->rows[y] << c;
  full_lines(b, rows, cols);
}

// Giro de 90° en el sentido de las agujas del reloj; cada casilla conserva su tipo.
void piece_rotate(Piece *p) {
  uint8_t j, y, h = p->h;
  for (j = 0; j < p->n; ++j) {
    y = p->cy[j];
    p->cy[j] = p->cx[j];
    p->cx[j] = h - 1 - y;
  }
  p->h = p->w; p->w = h;
  p->rows[0] = p->rows[1] = p->rows[2] = 0;
  for (j = 0; j < p->n; ++j) p->rows[p->cy[j]] |= 1 << p->cx[j];
}

uint8_t piece_rotation_fits(const Piece *p) {
  static Piece q;
  uint8_t k;
  q = *p;
  for (k = 0; k < 3; ++k) { piece_rotate(&q); if (fits_anywhere(occ, &q)) return 1; }
  return 0;
}

void place_piece(const Piece *p, uint8_t r, uint8_t c) {
  uint8_t j, k, kind;
  for (j = 0; j < p->n; ++j) {
    k = (r + p->cy[j]) * N + c + p->cx[j];
    kind = p->kind[j];
    occ[r + p->cy[j]] |= 1 << (c + p->cx[j]);
    cellc[k] = kind == K_STONE ? C_STONE : p->color;
    cellk[k] = kind;
  }
}

// Elimina las líneas completas del tablero real: la piedra entera se agrieta,
// las bombas que caen arrasan su zona de 3x3 (en cadena) y se cuentan las líneas de un solo color.
void resolve_clear(void) {
  static uint8_t queue[N * N], exploded[N];
  uint8_t r, c, k, L, bit, qn = 0, rows, cols, c0, ok;
  int8_t dr, dc, rr, cc;

  memset(&clr, 0, sizeof clr);
  clr.nlines = full_lines(occ, &rows, &cols);
  clr.linerows = rows; clr.linecols = cols;
  if (!clr.nlines) return;

  for (r = 0; r < N; ++r) {
    L = (rows & (1 << r)) ? 0xFF : cols;
    for (c = 0; c < N; ++c) {
      bit = 1 << c;
      if (!(L & bit)) continue;
      k = r * N + c;
      if (cellk[k] == K_STONE) clr.cracked[r] |= bit;
      else {
        clr.removed[r] |= bit;
        if (cellk[k] == K_BOMB) queue[qn++] = k;
      }
    }
  }

  // líneas de un solo color (la piedra y los bloques grises iniciales no cuentan)
  for (r = 0; r < N; ++r) if (rows & (1 << r)) {
    c0 = cellc[r * N]; ok = c0 != C_GREY && c0 != C_STONE;
    for (c = 1; c < N && ok; ++c) if (cellc[r * N + c] != c0) ok = 0;
    if (ok) ++clr.mono;
  }
  for (c = 0; c < N; ++c) if (cols & (1 << c)) {
    c0 = cellc[c]; ok = c0 != C_GREY && c0 != C_STONE;
    for (r = 1; r < N && ok; ++r) if (cellc[r * N + c] != c0) ok = 0;
    if (ok) ++clr.mono;
  }

  // bombas en cadena
  memset(exploded, 0, N);
  while (qn) {
    k = queue[--qn];
    r = k >> 3; c = k & 7;
    if (exploded[r] & (1 << c)) continue;
    exploded[r] |= 1 << c;
    ++clr.bombs;
    for (dr = -1; dr <= 1; ++dr)
      for (dc = -1; dc <= 1; ++dc) {
        rr = r + dr; cc = c + dc;
        if (rr < 0 || cc < 0 || rr >= N || cc >= N) continue;
        bit = 1 << cc;
        if (!(occ[rr] & bit) || (clr.removed[rr] & bit)) continue;
        clr.removed[rr] |= bit; clr.boom[rr] |= bit; clr.cracked[rr] &= ~bit;
        ++clr.boomcells;
        if (cellk[rr * N + cc] == K_BOMB) queue[qn++] = rr * N + cc;
      }
  }

  // aplicar
  clr.clean = 1;
  for (r = 0; r < N; ++r) {
    for (c = 0; c < N; ++c) {
      bit = 1 << c; k = r * N + c;
      if (clr.removed[r] & bit) { occ[r] &= ~bit; cellc[k] = C_EMPTY; cellk[k] = K_NONE; }
      else if (clr.cracked[r] & bit) cellk[k] = K_CRACKED;
    }
    if (occ[r]) clr.clean = 0;
  }
}

// Tablero inicial: 5 piezas grises repartidas sin completar ninguna línea.
void board_seed(void) {
  static Piece p;
  uint8_t attempt, tries, placed, j, filled, rows, cols, y;
  int8_t r, c;
  uint8_t b[N];
  for (attempt = 0; attempt < 60; ++attempt) {
    memset(occ, 0, N); memset(cellc, 0, N * N); memset(cellk, 0, N * N);
    placed = 0; filled = 0;
    for (tries = 0; tries < 200 && placed < 5; ++tries) {
      random_normal(&p);
      if (p.fam == 0) continue;               // sin cuadrados de 3x3
      r = rnd_below(N - p.h + 1); c = rnd_below(N - p.w + 1);
      if (!can_place(occ, &p, r, c)) continue;
      memcpy(b, occ, N);
      for (y = 0; y < p.h; ++y) b[r + y] |= p.rows[y] << c;
      if (full_lines(b, &rows, &cols)) continue;
      memcpy(occ, b, N);
      for (j = 0; j < p.n; ++j) cellc[(r + p.cy[j]) * N + c + p.cx[j]] = C_GREY;
      filled += p.n; ++placed;
    }
    if (filled >= 14 && filled <= 24) return;
  }
}

// ---------- Reparto con ayuda ----------
// Igual que la web: las primeras rondas siempre ayuda; después un 80 % que baja 3 puntos por ronda hasta el 20 %.
uint8_t assist_chance(uint8_t round) {
  int16_t v;
  if (round <= 3) return 100;
  v = 80 - 3 * (int16_t)(round - 4);
  return v < 20 ? 20 : (uint8_t)v;
}
uint8_t assist_min_forgiveness(uint8_t round) { return round <= 3 ? 90 : 75; }

// Colocación simulada: solo ocupación, con eliminación de líneas.
static void sim_place(uint8_t *d, const uint8_t *s, const Piece *p, uint8_t r, uint8_t c) {
  uint8_t i, cm = 0xFF, v;
  const uint8_t *sh = &shl[c * 8];
  memcpy(d, s, N);
  d[r] |= sh[p->rows[0]];
  if (p->h > 1) d[r + 1] |= sh[p->rows[1]];
  if (p->h > 2) d[r + 2] |= sh[p->rows[2]];
  for (i = 0; i < N; ++i) cm &= d[i];
  cm = ~cm;
  for (i = 0; i < N; ++i) { v = d[i]; d[i] = v == 0xFF ? 0 : (v & cm); }
}

// ¿Existe un orden y unas posiciones para colocar TODAS las piezas una tras otra?
// Dos límites de trabajo (en colocaciones simuladas): 'budget' para esta búsqueda y
// 'deal_budget' para todo el reparto. Si se agotan, se responde que no (conservador)
// y el reparto acaba construyendo una bandeja que cabe con seguridad.
static int16_t budget, deal_budget;
static uint8_t can_place_all(const uint8_t *b, const Piece **ps, uint8_t n) {
  uint8_t i, j, k, r, c, m;
  uint8_t nb[N], vm[N];
  const Piece *rest[3];
  const Piece *p;
  if (n == 1) return fits_anywhere(b, ps[0]);
  for (i = 0; i < n; ++i) {
    p = ps[i];
    for (j = 0, k = 0; j < n; ++j) if (j != i) rest[k++] = ps[j];
    if (!valid_positions(b, p, vm)) return 0;   // si una pieza no cabe ya, ningún orden sirve
    for (r = 0; r < N; ++r)
      for (c = 0, m = vm[r]; m; ++c, m >>= 1) {
        if (!(m & 1)) continue;
        if (--budget < 0 || --deal_budget < 0) return 0;
        sim_place(nb, b, p, r, c);
        if (can_place_all(nb, rest, n - 1)) return 1;
      }
  }
  return 0;
}

// Margen de error: de las primeras jugadas posibles, qué % deja sitio a las demás piezas.
// En el Spectrum se mide con una muestra de hasta 8 jugadas en vez de todas.
#define SAMPLES 8
static uint8_t sp_r[3 * N * N], sp_c[3 * N * N], sp_i[3 * N * N];
static uint8_t forgiveness(const Piece **ps, uint8_t n) {
  uint8_t i, j, k, r, c, m, total = 0, tested, ok = 0, t, idx;
  uint8_t nb[N], vm[N];
  const Piece *rest[3];
  if (n == 1) return 100;
  for (i = 0; i < n; ++i) {
    valid_positions(occ, ps[i], vm);
    for (r = 0; r < N; ++r)
      for (c = 0, m = vm[r]; m; ++c, m >>= 1)
        if (m & 1) { sp_r[total] = r; sp_c[total] = c; sp_i[total] = i; ++total; }
  }
  if (!total) return 0;
  tested = total < SAMPLES ? total : SAMPLES;
  for (t = 0; t < tested; ++t) {
    idx = total <= SAMPLES ? t : rnd_below(total);
    i = sp_i[idx];
    for (j = 0, k = 0; j < n; ++j) if (j != i) rest[k++] = ps[j];
    sim_place(nb, occ, ps[i], sp_r[idx], sp_c[idx]);
    budget = 40;
    if (can_place_all(nb, rest, n - 1)) ++ok;
    if (deal_budget < 0) return 0;
  }
  return (uint8_t)((uint16_t)ok * 100 / tested);
}

static void shuffle_pieces(Piece *a, uint8_t n) {
  static Piece tmp;
  uint8_t i, j;
  for (i = n - 1; i > 0; --i) {
    j = rnd_below(i + 1);
    if (j != i) { tmp = a[i]; a[i] = a[j]; a[j] = tmp; }
  }
}

// Construye la bandeja pieza a pieza sobre un tablero simulado: primero piezas normales
// y, si ninguna cabe, piezas de ayuda (de mayor a menor). Siempre caben en algún orden.
static void build_fitting(Piece *out, uint8_t count) {
  static Piece tmp;
  uint8_t g[N], ng[N], vm[N], s, k, found, start, total, pref, pick, rows, cols, y, r, c, m;
  memcpy(g, occ, N);
  for (s = 0; s < count; ++s) {
    found = 0;
    for (k = 0; k < 8 && !found; ++k) { random_normal(&tmp); found = fits_anywhere(g, &tmp); }
    if (!found) {
      start = rnd_below(NORMAL_SHAPES);
      for (k = 0; k < NORMAL_SHAPES && !found; ++k) { make_piece(&tmp, (start + k) % NORMAL_SHAPES); found = fits_anywhere(g, &tmp); }
    }
    for (k = 0; k < HELPER_SHAPES && !found; ++k) { make_piece(&tmp, HELPER_FIRST + k); found = fits_anywhere(g, &tmp); }
    if (!found) { random_normal(&tmp); out[s] = tmp; continue; }   // tablero lleno: no debería ocurrir

    // elige hueco: si alguno completa líneas, entre esos
    valid_positions(g, &tmp, vm);
    total = 0; pref = 0;
    for (r = 0; r < N; ++r)
      for (c = 0, m = vm[r]; m; ++c, m >>= 1) if (m & 1) {
        memcpy(ng, g, N);
        for (y = 0; y < tmp.h; ++y) ng[r + y] |= tmp.rows[y] << c;
        if (full_lines(ng, &rows, &cols)) ++pref;
        ++total;
      }
    pick = rnd_below(pref ? pref : total);
    for (r = 0; r < N; ++r)
      for (c = 0, m = vm[r]; m; ++c, m >>= 1) if (m & 1) {
        if (pref) {
          memcpy(ng, g, N);
          for (y = 0; y < tmp.h; ++y) ng[r + y] |= tmp.rows[y] << c;
          if (!full_lines(ng, &rows, &cols)) continue;
        }
        if (pick-- == 0) { sim_place(ng, g, &tmp, r, c); memcpy(g, ng, N); goto chosen; }
      }
  chosen:
    out[s] = tmp;
  }
  shuffle_pieces(out, count);
}

// Bandeja con ayuda: busca piezas que quepan todas y que perdonen jugadas poco precisas.
// Empieza con piezas normales al azar y, si el tablero está apretado, mete piezas de ayuda.
static void assisted(Piece *out, uint8_t count, uint8_t minpct) {
  static const uint8_t tries[4] = { 6, 3, 3, 3 };
  const Piece *ps[3];
  uint8_t h, k, i;
  deal_budget = 700;   // unos 0,5 s de Z80 en el peor caso
  for (h = 0; h <= count; ++h)
    for (k = 0; k < tries[h]; ++k) {
      if (deal_budget < 0) goto fallback;
      for (i = 0; i < count; ++i) {
        if (i < count - h) random_normal(&out[i]); else random_helper(&out[i]);
        ps[i] = &out[i];
      }
      budget = 120;
      if (can_place_all(occ, ps, count) && forgiveness(ps, count) >= minpct) {
        shuffle_pieces(out, count);
        return;
      }
    }
fallback:
  build_fitting(out, count);
}

// Casillas especiales: bombas desde la ronda 2 (25 %); piedra desde la 5, cada vez más frecuente.
static void decorate(Piece *out, uint8_t count, uint8_t round) {
  uint8_t i, j, cand, pick, want, stone;
  Piece *p;
  if (round >= 2 && rnd_below(100) < 25) {
    p = &out[rnd_below(count)];
    p->kind[rnd_below(p->n)] = K_BOMB;
  }
  stone = round < 5 ? 0 : (15 + 3 * (round - 5) > 50 ? 50 : 15 + 3 * (round - 5));
  if (stone && rnd_below(100) < stone) {
    for (i = 0, cand = 0; i < count; ++i) if (out[i].n >= 4) ++cand;
    if (!cand) return;
    pick = rnd_below(cand);
    for (i = 0; i < count; ++i) if (out[i].n >= 4 && pick-- == 0) break;
    p = &out[i];
    want = p->n >= 5 ? 2 : 1;
    for (j = rnd_below(p->n); want; j = (j + 1) % p->n)
      if (p->kind[j] == K_NONE) { p->kind[j] = K_STONE; --want; }
  }
}

void deal_pieces(Piece *out, uint8_t count, uint8_t round, uint8_t assist) {
  uint8_t i;
  if (assist) assisted(out, count, assist_min_forgiveness(round));
  else for (i = 0; i < count; ++i) random_normal(&out[i]);
  decorate(out, count, round);
}
