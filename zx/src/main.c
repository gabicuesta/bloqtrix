// BLOQTRIX ZX — versión para ZX Spectrum 48K / 128K.
// Pantalla: tablero de 8x8 casillas de 16x16 píxeles a la izquierda; marcador, racha,
// ayudas y las 3 piezas a la derecha.
#include <im2.h>
#include <intrinsic.h>
#include <z80.h>
#include <string.h>
#include <stdint.h>
#include "engine.h"
#include "gfx.h"
#include "sound.h"
#include "lang.h"

#define BR 4          // fila (en bloques de 8x8) donde empieza el tablero
#define BC 1          // columna donde empieza el tablero
#define PANEL 19      // columna del panel derecho
#define MAX_POWER 3
#define MAX_STREAK 8

#define VERSION "V1.0"
#ifdef MODEL128
#define MODEL_NAME "128K AY"
#else
#define MODEL_NAME "48K"
#endif

uint8_t lang;   // 0 = español, 1 = inglés (tecla L en el título)

// Atributos de texto
#define T_LABEL (A_BRIGHT | INK(Z_CYAN))
#define T_VALUE (A_BRIGHT | INK(Z_WHITE))
#define T_GOLD  (A_BRIGHT | INK(Z_YELLOW))
#define T_DIM   INK(Z_WHITE)
#define T_KEY   (A_BRIGHT | PAPER(Z_YELLOW) | INK(Z_BLACK))
#define T_GOOD  (A_BRIGHT | INK(Z_GREEN))
#define T_BAD   (A_BRIGHT | INK(Z_RED))

// ---------- Interrupción de 50 Hz (modo IM 2, tabla en 0xFC00) ----------
volatile uint8_t frames;
IM2_DEFINE_ISR(isr) {
  ++frames;
  snd_frame();
}
static void setup_im2(void) {
  intrinsic_di();
  im2_init((void *)0xFC00);
  memset((void *)0xFC00, 0xFD, 257);
  z80_bpoke(0xFDFD, 0xC3);
  z80_wpoke(0xFDFE, (unsigned int)isr);
  intrinsic_ei();
}
static void wait_frames(uint8_t n) { while (n--) intrinsic_halt(); }

// ---------- Entrada ----------
#define K_UP    0x01
#define K_DOWN  0x02
#define K_LEFT  0x04
#define K_RIGHT 0x08
#define K_FIRE  0x10
#define K_SWAP  0x20
#define K_ROT   0x40
#define K_NEXT  0x80
#define K2_1 0x01
#define K2_2 0x02
#define K2_3 0x04
#define K2_S 0x08
#define K2_M 0x10
#define K2_L 0x20

static uint8_t keys, keys_prev, keys2, keys2_prev, joy, joy_prev, kempston, kempston_checked;
#define PRESSED(m)  ((keys & (m)) && !(keys_prev & (m)))
#define PRESSED2(m) ((keys2 & (m)) && !(keys2_prev & (m)))
#define JPRESSED(m) ((joy & (m)) && !(joy_prev & (m)))
#define JRELEASED(m) (!(joy & (m)) && (joy_prev & (m)))

static void read_input(void) {
  uint8_t r;
  keys_prev = keys; keys2_prev = keys2; joy_prev = joy;
  keys = 0; keys2 = 0;
  r = ~z80_inp(0xFBFE); if (r & 0x01) keys |= K_UP;                                    // Q
  r = ~z80_inp(0xFDFE); if (r & 0x01) keys |= K_DOWN;                                  // A
                        if (r & 0x10) keys |= K_ROT;                                   // G
                        if (r & 0x02) keys2 |= K2_S;                                   // S
  r = ~z80_inp(0xDFFE); if (r & 0x02) keys |= K_LEFT; if (r & 0x01) keys |= K_RIGHT;   // O P
  r = ~z80_inp(0xEFFE); if (r & 0x08) keys |= K_UP;   if (r & 0x10) keys |= K_DOWN;    // 7 6
                        if (r & 0x04) keys |= K_RIGHT; if (r & 0x01) keys |= K_FIRE;   // 8 0
  r = ~z80_inp(0xF7FE); if (r & 0x10) keys |= K_LEFT;                                  // 5
                        if (r & 0x01) keys2 |= K2_1; if (r & 0x02) keys2 |= K2_2; if (r & 0x04) keys2 |= K2_3;
  r = ~z80_inp(0x7FFE); if (r & 0x01) keys |= K_FIRE; if (r & 0x08) keys |= K_NEXT;    // ESPACIO N
                        if (r & 0x04) keys2 |= K2_M;                                   // M
  r = ~z80_inp(0xBFFE); if (r & 0x01) keys |= K_FIRE;                                  // ENTER
                        if (r & 0x02) keys2 |= K2_L;                                   // L
  r = ~z80_inp(0xFEFE); if (r & 0x08) keys |= K_SWAP;                                  // C
  // Kempston: bit 0 derecha, 1 izquierda, 2 abajo, 3 arriba, 4 disparo
  if (kempston) {
    r = z80_inp(0x001F);
    joy = 0;
    if (r & 0x01) joy |= K_RIGHT;
    if (r & 0x02) joy |= K_LEFT;
    if (r & 0x04) joy |= K_DOWN;
    if (r & 0x08) joy |= K_UP;
    if (r & 0x10) joy |= K_FIRE;
  }
}

// Un Kempston en reposo devuelve 0. Sin joystick, el puerto 0x1F devuelve lo que el ULA esté
// leyendo de la pantalla en ese momento ("bus flotante"), así que se lee durante más de un
// fotograma con la pantalla de título ya dibujada: cualquier valor distinto de 0 descarta el joystick.
static void detect_kempston(void) {
  uint16_t i;
  kempston = 1;
  for (i = 0; i < 3000; ++i)
    if (z80_inp(0x001F)) { kempston = 0; return; }
}

// Repetición de dirección: mueve al pulsar y, si se mantiene, cada 3 fotogramas tras una pausa.
static uint8_t rep[4];
static uint8_t repeat_dir(uint8_t held, uint8_t i) {
  if (!held) { rep[i] = 0; return 0; }
  if (!rep[i]) { rep[i] = 1; return 1; }
  if (++rep[i] >= 13) { rep[i] = 10; return 1; }
  return 0;
}

// ---------- Texto y números ----------
static char nbuf[12];
static char *u32str(uint32_t v) {
  char *p = nbuf + 11;
  *p = 0;
  do { *--p = '0' + (uint8_t)(v % 10); v /= 10; } while (v);
  return p;
}
static void print_right(uint8_t row, uint8_t col_end, const char *s, uint8_t a) {
  print_at(row, col_end + 1 - text_len(s), s, a);
}
static char sbuf[24];
static char *cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static void print_center(uint8_t row, const char *s, uint8_t a) { print_at(row, (32 - text_len(s)) >> 1, s, a); }

// ---------- Estado de la partida ----------
static Piece tray[3], spare[3];
static uint8_t present[3], sel;
static int8_t cur_r, cur_c;
static uint32_t score, best, best_at_start;
static uint8_t round_no, streak, swaps, rotates, stuck, over;
static uint16_t st_lines, st_pieces;
static uint8_t st_beststreak, st_bombs, st_cleans;
static uint8_t spark;

// sombra de la pieza seleccionada y líneas que completaría
static uint8_t gmask[N], fmask[N], gvalid, ghost_color;

// mensajes bajo el tablero
static char msg2q[4][24];
static uint8_t msg2n, msg2i, msg1_timer, msg2_timer;

// ---------- Dibujo del tablero ----------
static void draw_cell(uint8_t r, uint8_t c) {
  uint8_t k = r * N + c, bit = 1 << c, a;
  const uint8_t *bmp;
  if (occ[r] & bit) {
    switch (cellk[k]) {
      case K_BOMB: bmp = bmp_bomb[spark]; break;
      case K_STONE: bmp = bmp_stone; break;
      case K_CRACKED: bmp = bmp_cracked; break;
      default: bmp = bmp_block;
    }
    a = color_paper_attr[cellc[k]];
    if (gmask[r] & bit) a = A_BRIGHT | PAPER(Z_RED);   // la sombra choca con este bloque
    else if (fmask[r] & bit) a |= A_FLASH;             // línea que se completaría
  } else if (gmask[r] & bit) {
    bmp = bmp_ghost;
    a = gvalid ? color_ink_attr[ghost_color] : T_BAD;
    if (fmask[r] & bit) a |= A_FLASH;
  } else {
    bmp = bmp_empty;
    a = (fmask[r] & bit) ? (A_FLASH | INK(Z_BLUE)) : INK(Z_BLUE);
  }
  draw16(BR + r * 2, BC + c * 2, bmp, a);
}

static void draw_board(void) {
  uint8_t r, c;
  for (r = 0; r < N; ++r) for (c = 0; c < N; ++c) draw_cell(r, c);
}

// Recalcula la sombra y redibuja solo las casillas que cambian.
static void update_ghost(void) {
  static uint8_t old_g[N], old_f[N];
  uint8_t r, c, j, rows, cols, ch;
  const Piece *p;
  memcpy(old_g, gmask, N); memcpy(old_f, fmask, N);
  memset(gmask, 0, N); memset(fmask, 0, N); gvalid = 0;
  if (!over && present[sel]) {
    p = &tray[sel];
    ghost_color = p->color;
    for (j = 0; j < p->n; ++j) gmask[cur_r + p->cy[j]] |= 1 << (cur_c + p->cx[j]);
    gvalid = can_place(occ, p, cur_r, cur_c);
    if (gvalid) {
      preview_lines(p, cur_r, cur_c, &rows, &cols);
      for (r = 0; r < N; ++r) fmask[r] = (rows & (1 << r)) ? 0xFF : cols;
    }
  }
  for (r = 0; r < N; ++r) {
    ch = old_g[r] | old_f[r] | gmask[r] | fmask[r];
    for (c = 0; ch; ++c, ch >>= 1) if (ch & 1) draw_cell(r, c);
  }
}

static void animate_bombs(void) {
  uint8_t k;
  for (k = 0; k < N * N; ++k) if (cellk[k] == K_BOMB) draw_cell(k >> 3, k & 7);
}

// ---------- Panel ----------
static void draw_powers(void) {
  uint8_t a;
  a = (stuck && swaps) ? (A_FLASH | T_KEY) : T_KEY;
  print_at(9, PANEL, "C", a);
  clear_area(9, PANEL + 2, 9, 1, 0);
  print_at(9, PANEL + 2, TX(S_SWAP), swaps ? T_VALUE : T_DIM);
  print_at(9, 31, u32str(swaps), swaps ? T_GOLD : T_DIM);
  a = (stuck && rotates) ? (A_FLASH | T_KEY) : T_KEY;
  print_at(10, PANEL, "G", a);
  clear_area(10, PANEL + 2, 9, 1, 0);
  print_at(10, PANEL + 2, TX(S_ROTATE), rotates ? T_VALUE : T_DIM);
  print_at(10, 31, u32str(rotates), rotates ? T_GOLD : T_DIM);
}

// Tecla M (solo 128K): bajo Cambiar y Girar, con el estado de la música.
static void draw_music(void) {
#ifdef MODEL128
  print_at(11, PANEL, "M", T_KEY);
  print_at(11, PANEL + 2, TX(S_MUSIC), music_on ? T_VALUE : T_DIM);
  print_at(11, 29, music_on ? TX(S_ON) : TX(S_OFF), music_on ? T_GOLD : T_DIM);
#endif
}

static void draw_streak(void) {
  uint8_t m = streak > MAX_STREAK ? MAX_STREAK : streak;
  sbuf[0] = 'x'; sbuf[1] = '0' + m; sbuf[2] = 0;
  print_at(7, PANEL, TX(S_STREAK), T_LABEL);
  print_tall(6, 29, sbuf, streak ? (A_BRIGHT | INK(Z_RED)) : T_DIM);
}

static void draw_score(void) {
  clear_area(1, PANEL, 13, 2, 0);
  print_tall(1, PANEL, u32str(score), T_GOLD);
  clear_area(5, PANEL, 13, 1, 0);
  print_at(5, PANEL, u32str(score > best ? score : best), T_VALUE);
}

static void draw_slot(uint8_t s) {
  uint8_t row0 = 12 + s * 4, j, oy, ox, kind, fits, a;
  const Piece *p = &tray[s];
  const uint8_t *bmp;
  clear_area(row0, PANEL, 13, 3, 0);
  sbuf[0] = '1' + s; sbuf[1] = 0;
  print_at(row0 + 1, PANEL, sbuf, (present[s] && sel == s) ? T_KEY : T_DIM);
  if (present[s] && sel == s) print_at(row0 + 1, PANEL + 1, ">", T_GOLD);
  if (!present[s]) return;
  fits = fits_anywhere(occ, p);
  oy = (3 - p->h) >> 1; ox = (3 - p->w) >> 1;
  for (j = 0; j < p->n; ++j) {
    kind = p->kind[j];
    if (!fits) { bmp = bmp8_ghost; a = color_ink_attr[p->color]; }
    else if (kind == K_BOMB) { bmp = bmp8_bomb; a = color_paper_attr[p->color]; }
    else if (kind == K_STONE) { bmp = bmp8_stone; a = color_paper_attr[C_STONE]; }
    else { bmp = bmp8_block; a = color_paper_attr[p->color]; }
    draw8(row0 + oy + p->cy[j], PANEL + 3 + ox + p->cx[j], bmp, a);
  }
}
static void draw_tray(void) { draw_slot(0); draw_slot(1); draw_slot(2); }

// ---------- Mensajes ----------
static void msg1(const char *s, uint8_t a) {
  clear_area(21, 0, 18, 1, 0);
  print_at(21, 0, s, a);
  msg1_timer = 75;
}
static void msg2_push(const char *s) {
  if (msg2n < 4) { strcpy(msg2q[msg2n], s); ++msg2n; }
}
static void draw_stuck(void) {
  clear_area(22, 0, 18, 1, 0);
  if (stuck) print_at(22, 0, TX(S_STUCK), A_FLASH | T_GOLD);
}
static void tick_messages(void) {
  if (msg1_timer && !--msg1_timer) clear_area(21, 0, 18, 1, 0);
  if (msg2_timer) { if (--msg2_timer) return; clear_area(22, 0, 18, 1, 0); draw_stuck(); }
  if (msg2i < msg2n) {
    clear_area(22, 0, 18, 1, 0);
    print_at(22, 0, msg2q[msg2i], T_GOLD);
    ++msg2i; msg2_timer = 45;
  } else { msg2n = msg2i = 0; }
}

// ---------- Reglas ----------
static void clamp_cursor(void) {
  const Piece *p = &tray[sel];
  if (cur_r > N - p->h) cur_r = N - p->h;
  if (cur_c > N - p->w) cur_c = N - p->w;
  if (cur_r < 0) cur_r = 0;
  if (cur_c < 0) cur_c = 0;
}

static void select_slot(uint8_t s) {
  if (!present[s]) return;
  sel = s;
  clamp_cursor();
}
static void select_next_present(void) {
  uint8_t i;
  if (present[sel]) return;
  for (i = 1; i <= 3; ++i) if (present[(sel + i) % 3]) { sel = (sel + i) % 3; break; }
  clamp_cursor();
}
static void cycle_piece(int8_t dir) {
  uint8_t i, s;
  for (i = 1; i <= 2; ++i) {
    s = (uint8_t)(sel + (dir > 0 ? i : 3 - i)) % 3;
    if (present[s]) { select_slot(s); return; }
  }
}

static void check_over(void) {
  uint8_t i, any = 0, blocked = 1, rescue = 0;
  for (i = 0; i < 3; ++i) if (present[i]) { any = 1; if (fits_anywhere(occ, &tray[i])) blocked = 0; }
  if (!any) blocked = 0;
  if (blocked) {
    if (swaps) rescue = 1;
    else if (rotates) for (i = 0; i < 3; ++i) if (present[i] && piece_rotation_fits(&tray[i])) rescue = 1;
  }
  stuck = blocked && rescue;
  over = blocked && !rescue;
}

static void deal_round(void) {
  uint8_t assisted;
  clear_area(22, 0, 18, 1, 0);
  print_at(22, 0, TX(S_DEALING), T_DIM);
  ++round_no;
  assisted = rnd_below(100) < assist_chance(round_no);
  deal_pieces(tray, 3, round_no, assisted);
  present[0] = present[1] = present[2] = 1;
  sel = 0;
  clear_area(22, 0, 18, 1, 0);
}

static void new_game(void) {
  board_seed();
  score = 0; best_at_start = best;
  round_no = 0; streak = 0; swaps = 1; rotates = 1; stuck = over = 0;
  st_lines = st_pieces = 0; st_beststreak = st_bombs = st_cleans = 0;
  msg2n = msg2i = msg1_timer = msg2_timer = 0;
  memset(gmask, 0, N); memset(fmask, 0, N);
  deal_round();
  cur_r = 3; cur_c = 3;
  clamp_cursor();
}

static void draw_game_screen(void) {
  gfx_cls();
  print_tall(0, BC, "BLOQ", T_VALUE);
  print_tall(0, BC + 4, "TRIX", T_GOLD);
  print_at(1, BC + 10, MODEL_NAME, T_DIM);
  draw_frame(BR, BC, 16, 16, A_BRIGHT | INK(Z_BLUE));
  print_at(0, PANEL, TX(S_SCORE), T_LABEL);
  print_at(4, PANEL, TX(S_BEST), T_LABEL);
  print_at(23, 0, TX(S_HINT), T_DIM);
  draw_score(); draw_streak(); draw_powers(); draw_music(); draw_tray();
  draw_board();
  update_ghost();
}

static void flash_attr_mask(const uint8_t *mask, uint8_t a) {
  uint8_t r, c, m;
  for (r = 0; r < N; ++r)
    for (c = 0, m = mask[r]; m; ++c, m >>= 1)
      if (m & 1) attr16(BR + r * 2, BC + c * 2, a);
}

static void game_over_screen(void);

static void after_change(void) {
  select_next_present();
  check_over();
  draw_powers(); draw_tray(); draw_stuck();
  update_ghost();
  if (over) game_over_screen();
}

static void do_place(void) {
  Piece *p = &tray[sel];
  uint8_t n, mult, j, rows, cols, fan = 0, earned = 0;
  uint8_t lm[N];
  uint16_t prev;
  uint32_t gain;
  char *d;

  if (!can_place(occ, p, cur_r, cur_c)) { snd_error(); return; }
  place_piece(p, cur_r, cur_c);
  present[sel] = 0;
  ++st_pieces;
  score += p->n;
  memset(gmask, 0, N); memset(fmask, 0, N);
  for (j = 0; j < p->n; ++j) draw_cell(cur_r + p->cy[j], cur_c + p->cx[j]);
  snd_place();

  n = full_lines(occ, &rows, &cols);
  if (n) {
    // las líneas se iluminan en blanco y suenan
    for (j = 0; j < N; ++j) lm[j] = (rows & (1 << j)) ? 0xFF : cols;
    flash_attr_mask(lm, A_BRIGHT | PAPER(Z_WHITE));
    ++streak;
    snd_lines(n, streak);
    wait_frames(6);
    resolve_clear();
    if (clr.bombs) {
      snd_boom();
      for (j = 0; j < 8; ++j) {
        flash_attr_mask(clr.boom, (j & 1) ? (A_BRIGHT | PAPER(Z_RED)) : (A_BRIGHT | PAPER(Z_YELLOW)));
        wait_frames(2);
      }
    }
    draw_board();

    mult = streak > MAX_STREAK ? MAX_STREAK : streak;
    prev = st_lines;
    st_lines += n;
    if (streak > st_beststreak) st_beststreak = streak;
    gain = (uint32_t)10 * n * n * mult;
    score += gain;
    d = cat(sbuf, "+"); d = cat(d, u32str(gain));
    if (n == 2) d = cat(d, TX(S_DOUBLE));
    else if (n == 3) d = cat(d, TX(S_TRIPLE));
    else if (n >= 4) { d = cat(d, " "); d = cat(d, u32str(n)); d = cat(d, TX(S_LINES_SUFFIX)); }
    if (mult > 1) { d = cat(d, " x"); d = cat(d, u32str(mult)); }
    msg1(sbuf, A_BRIGHT | INK(Z_WHITE));

    if (clr.bombs) {
      gain = (uint32_t)clr.boomcells * 2; score += gain; st_bombs += clr.bombs;
      d = cat(sbuf, TX(S_BOOM)); cat(d, u32str(gain)); msg2_push(sbuf);
    }
    if (clr.mono) {
      gain = (uint32_t)clr.mono * 100; score += gain;
      d = cat(sbuf, TX(S_COLOUR)); cat(d, u32str(gain)); msg2_push(sbuf);
      fan = 1;
    }
    if (clr.clean) {
      score += 300; ++st_cleans;
      msg2_push(TX(S_CLEAN));
      fan = 1;
    }
    if (fan) snd_fanfare();
    if (streak % 3 == 0 && swaps < MAX_POWER) { ++swaps; msg2_push(TX(S_PLUS_SWAP)); earned = 1; }
    if (clr.clean && swaps < MAX_POWER) { ++swaps; earned = 1; }
    if (st_lines / 8 > prev / 8 && rotates < MAX_POWER) { ++rotates; msg2_push(TX(S_PLUS_ROT)); earned = 1; }
    if (earned) snd_power();
  } else {
    streak = 0;
  }

  if (!present[0] && !present[1] && !present[2]) deal_round();
  draw_score(); draw_streak();
  after_change();
}

static void do_swap(void) {
  uint8_t open[3], cnt = 0, i;
  if (!swaps || over) return;
  --swaps;
  for (i = 0; i < 3; ++i) if (present[i]) open[cnt++] = i;
  deal_pieces(spare, cnt, round_no, 1);
  for (i = 0; i < cnt; ++i) tray[open[i]] = spare[i];
  clamp_cursor();
  snd_swap();
  after_change();
}

static void do_rotate(void) {
  if (!rotates || over || !present[sel]) return;
  --rotates;
  piece_rotate(&tray[sel]);
  clamp_cursor();
  snd_rotate();
  after_change();
}

// ---------- Fin de partida ----------
static void stat_line(uint8_t row, const char *label, uint32_t v, const char *prefix) {
  print_at(row, BC + 1, label, T_LABEL);
  if (prefix) { char *d = cat(sbuf, prefix); cat(d, u32str(v)); print_right(row, BC + 14, sbuf, T_VALUE); }
  else print_right(row, BC + 14, u32str(v), T_VALUE);
}

static void wait_fire_release(void) {
  do { intrinsic_halt(); read_input(); } while ((keys & K_FIRE) || (joy & K_FIRE));
}

static void game_over_screen(void) {
  uint8_t newrec = score > best_at_start;
  if (score > best) best = score;
  clear_area(BR, BC, 16, 16, 0);
  draw_frame(BR, BC, 16, 16, A_BRIGHT | INK(Z_RED));
  print_tall(BR + 2, BC + 3, TX(S_NOROOM), T_BAD);
  print_at(BR + 4, BC + 1, TX(S_SCORE), T_LABEL);
  print_right(BR + 4, BC + 14, u32str(score), T_GOLD);
  if (newrec) print_at(BR + 5, BC + 1, TX(S_NEWBEST), A_FLASH | T_GOOD);
  stat_line(BR + 7, TX(S_ST_LINES), st_lines, 0);
  stat_line(BR + 8, TX(S_ST_STREAK), st_beststreak, "x");
  stat_line(BR + 9, TX(S_ST_ROUNDS), round_no, 0);
  stat_line(BR + 10, TX(S_ST_PIECES), st_pieces, 0);
  stat_line(BR + 11, TX(S_ST_BOMBS), st_bombs, 0);
  stat_line(BR + 12, TX(S_ST_CLEAN), st_cleans, 0);
  print_at(BR + 14, BC + 1, TX(S_AGAIN), A_FLASH | T_GOLD);
  draw_score();
  snd_gameover();
  wait_fire_release();
}

// ---------- Título ----------
static void draw_title(void) {
  static const uint8_t deco[] = { C_YELLOW, C_CYAN, C_RED, C_GREEN, C_MAGENTA, C_BLUE, C_ORANGE, C_PINK, C_LIME, C_TEAL, C_WHITE, C_YELLOW };
  uint8_t i;
  gfx_cls();
  print_center(0, CH_COPY " 2026 GABRIEL CUESTA", T_DIM);
  print_huge(2, 8, "BLOQ", T_VALUE);
  print_huge(2, 16, "TRIX", T_GOLD);
  for (i = 0; i < 12; ++i) draw16(5, 4 + i * 2, (i == 5) ? bmp_bomb[0] : (i == 9 ? bmp_stone : bmp_block), color_paper_attr[deco[i]]);
  print_center(8, TX(S_SUBTITLE), T_LABEL);
  print_at(10, 1, TX(S_MOVE_KEYS), T_GOLD);   print_at(10, 20, TX(S_MOVE), T_VALUE);
  print_at(11, 1, "1 2 3 / N", T_GOLD);       print_at(11, 20, TX(S_PICK), T_VALUE);
  print_at(12, 1, TX(S_PLACE_KEYS), T_GOLD);  print_at(12, 20, TX(S_PLACE), T_VALUE);
  print_at(13, 1, "C", T_GOLD);               print_at(13, 20, TX(S_SWAP), T_VALUE);
  print_at(14, 1, "G", T_GOLD);               print_at(14, 20, TX(S_ROTATE), T_VALUE);
  print_at(15, 1, "S", T_GOLD);               print_at(15, 20, TX(S_SOUND), T_VALUE);
#ifdef MODEL128
  print_at(16, 1, "M", T_GOLD);               print_at(16, 20, TX(S_MUSIC_COLON), T_VALUE);
  print_at(16, 20 + text_len(TX(S_MUSIC_COLON)), music_on ? TX(S_ON) : TX(S_OFF), T_GOLD);
#endif
  if (!kempston_checked) { detect_kempston(); kempston_checked = 1; }
  if (kempston) { print_at(17, 1, "KEMPSTON", T_GOLD); print_at(17, 20, TX(S_FIRE), T_VALUE); }
  print_at(18, 1, "L", T_GOLD);               print_at(18, 20, TX(S_LANGUAGE), T_VALUE);
  print_at(19, 1, TX(S_RULE1), T_DIM);
  print_at(20, 1, TX(S_RULE2), T_DIM);
  print_center(22, TX(S_PRESS), A_FLASH | T_GOLD);
  print_at(23, 0, VERSION " " MODEL_NAME, T_DIM);
  if (best) { print_at(23, 18, TX(S_BEST), T_LABEL); print_right(23, 31, u32str(best), T_VALUE); }
}

static void title_screen(void) {
  uint8_t t = 0;
  draw_title();
  music_restart();
  wait_fire_release();
  for (;;) {
    intrinsic_halt();
    ++t;
    read_input();
    if (PRESSED(K_FIRE) || JPRESSED(K_FIRE)) break;
    if (PRESSED2(K2_S)) snd_on ^= 1;
    if (PRESSED2(K2_L)) { lang ^= 1; draw_title(); }
#ifdef MODEL128
    if (PRESSED2(K2_M)) { music_on ^= 1; print_at(16, 20 + text_len(TX(S_MUSIC_COLON)), music_on ? TX(S_ON) : TX(S_OFF), T_GOLD); }
#endif
    if ((t & 15) == 0) {   // la bomba del título chisporrotea
      spark ^= 1;
      draw16(5, 14, bmp_bomb[spark], color_paper_attr[C_BLUE]);
    }
  }
  rng_seed(((uint16_t)frames << 8) ^ t ^ 0x5EED);
  wait_fire_release();
}

// ---------- Partida ----------
static void game(void) {
  uint8_t moved, jhold = 0, jused = 0, t = 0, dirs;
  new_game();
  draw_game_screen();
  snd_fanfare();
#ifdef MODEL128
  music_restart();
#endif
  for (;;) {
    intrinsic_halt();
    read_input();
    tick_messages();
    if (++t >= 12) { t = 0; spark ^= 1; if (!over) animate_bombs(); }

    if (over) {
      if (PRESSED(K_FIRE) || JPRESSED(K_FIRE)) {
        wait_fire_release();
        new_game();
        draw_game_screen();
        snd_fanfare();
#ifdef MODEL128
        music_restart();
#endif
      }
      continue;
    }

    if (PRESSED2(K2_S)) snd_on ^= 1;
    if (PRESSED2(K2_M)) { music_on ^= 1; draw_music(); }

    moved = 0;
    if (PRESSED2(K2_1) && present[0]) { select_slot(0); moved = 1; draw_tray(); }
    if (PRESSED2(K2_2) && present[1]) { select_slot(1); moved = 1; draw_tray(); }
    if (PRESSED2(K2_3) && present[2]) { select_slot(2); moved = 1; draw_tray(); }
    if (PRESSED(K_NEXT)) { cycle_piece(1); moved = 1; draw_tray(); }
    if (PRESSED(K_SWAP)) { do_swap(); continue; }
    if (PRESSED(K_ROT)) { do_rotate(); continue; }

    // Kempston: mantener disparo + izquierda/derecha cambia de pieza; soltarlo sin moverse coloca.
    if (JPRESSED(K_FIRE)) { jhold = 1; jused = 0; }
    if (jhold) {
      if (JPRESSED(K_LEFT)) { cycle_piece(-1); jused = 1; moved = 1; draw_tray(); }
      if (JPRESSED(K_RIGHT)) { cycle_piece(1); jused = 1; moved = 1; draw_tray(); }
      if (JRELEASED(K_FIRE)) { jhold = 0; if (!jused) { do_place(); continue; } }
    }

    dirs = keys | (jhold ? 0 : joy);
    if (repeat_dir(dirs & K_UP, 0) && cur_r > 0) { --cur_r; moved = 1; }
    if (repeat_dir(dirs & K_DOWN, 1) && cur_r < N - tray[sel].h) { ++cur_r; moved = 1; }
    if (repeat_dir(dirs & K_LEFT, 2) && cur_c > 0) { --cur_c; moved = 1; }
    if (repeat_dir(dirs & K_RIGHT, 3) && cur_c < N - tray[sel].w) { ++cur_c; moved = 1; }
    if (moved) { update_ghost(); snd_move(); }

    if (PRESSED(K_FIRE)) do_place();
  }
}

void main(void) {
#ifdef MODEL128
  // Pone el ROM del 48 BASIC (la fuente está en 0x3D00) y el banco 0 en 0xC000.
  // En +2A/+3 el ROM se elige con el bit 2 de 0x1FFD junto con el bit 4 de 0x7FFD. En un
  // 128K/+2, 0x1FFD se decodifica como si fuera 0x7FFD, así que justo después se escribe
  // 0x7FFD con el valor bueno. Va en ensamblador y sin usar la pila: entre las dos escrituras,
  // un 128K tiene otro banco en 0xC000, que es donde está la pila (main está más abajo).
  __asm
    di
    ld bc,0x1ffd
    ld a,0x04
    out (c),a
    ld bc,0x7ffd
    ld a,0x10
    out (c),a
  __endasm;
#endif
  setup_im2();
  engine_init();
  gfx_init();
  snd_init();
  for (;;) {
    title_screen();
    game();
  }
}
