// BLOQTRIX ZX — sonido del 128K con el chip AY-3-8912 (puertos 0xFFFD / 0xBFFD).
// Reparto de canales:
//   A: melodía de la música
//   B: efectos "musicales" (líneas, fanfarria, ayuda ganada, game over); si está libre, el bajo
//   C: efectos de golpe y ruido (colocar, explosión, cambiar, girar, error, cursor)
// Cada canal de efectos tiene una cola de pasos (tono, volumen, caída, deslizamiento, ruido)
// que snd_frame() avanza 50 veces por segundo desde la interrupción.
#include <intrinsic.h>
#include <z80.h>
#include "sound.h"
#include "notes.h"

uint8_t snd_on = 1;
uint8_t music_on = 1;

#define F_TONE  1
#define F_NOISE 2
#define F_VIB   4

typedef struct {
  uint16_t period;
  int8_t slide;           // se suma al periodo cada fotograma (positivo = más grave)
  uint8_t frames, vol, decay, flags, noise;
} Step;

#define QLEN 32
typedef struct {
  Step q[QLEN];
  uint8_t head, tail, t;
  uint16_t per;
} Fx;

static Fx fxB, fxC;

static void ay(uint8_t reg, uint8_t val) {
  z80_outp(0xFFFD, reg);
  z80_outp(0xBFFD, val);
}

static uint16_t note_period(uint8_t m) {
  if (m > 96) m = 96;
  if (m < NOTE_FIRST) m = NOTE_FIRST;
  return note_ay[NOTE(m)];
}

// --- colas (se llaman con las interrupciones desactivadas) ---
static void fx_clear(Fx *f) { f->head = f->tail = 0; f->t = 0; }
static void push(Fx *f, uint16_t period, uint8_t frames, uint8_t vol, uint8_t decay, uint8_t flags, int8_t slide, uint8_t noise) {
  Step *s;
  uint8_t next = (f->tail + 1) & (QLEN - 1);
  if (next == f->head) return;          // cola llena: se descarta
  s = &f->q[f->tail];
  s->period = period; s->frames = frames; s->vol = vol; s->decay = decay;
  s->flags = flags; s->slide = slide; s->noise = noise;
  f->tail = next;
}
static void push_note(Fx *f, uint8_t m, uint8_t frames, uint8_t vol, uint8_t decay) {
  push(f, note_period(m), frames, vol, decay, F_TONE, 0, 0);
}
static void push_rest(Fx *f, uint8_t frames) { push(f, 0, frames, 0, 0, 0, 0, 0); }

// Avanza un fotograma; devuelve 1 si el canal tiene algo que sonar.
static uint8_t fx_tick(Fx *f, uint16_t *per, uint8_t *vol, uint8_t *flags, uint8_t *noise) {
  Step *s;
  uint8_t v, d;
  if (f->head == f->tail) return 0;
  s = &f->q[f->head];
  if (f->t == 0) f->per = s->period; else f->per += s->slide;
  *per = f->per;
  if ((s->flags & F_VIB) && (f->t & 2)) *per += 3;
  v = s->vol;
  if (s->decay) { d = f->t / s->decay; v = d >= v ? 0 : v - d; }
  *vol = v; *flags = s->flags; *noise = s->noise;
  if (++f->t >= s->frames) { f->t = 0; f->head = (f->head + 1) & (QLEN - 1); }
  return 1;
}

// --- música: 4 compases de corcheas (32 filas), melodía en A y bajo en B ---
#define ROW_FRAMES 7
static const uint8_t mel[32] = {
  76, 79, 81, 79, 76, 74, 72, 74,
  76, 76, 79, 76, 74, 72, 69,  0,
  72, 74, 76, 79, 81, 79, 76, 79,
  81, 84, 81, 79, 76, 74, 72,  0,
};
static const uint8_t bass[32] = {
  48, 60, 48, 60, 48, 60, 48, 60,
  45, 57, 45, 57, 45, 57, 45, 57,
  41, 53, 41, 53, 41, 53, 41, 53,
  43, 55, 43, 55, 43, 55, 43, 55,
};
static uint8_t mrow, mtick, mplaying;

void music_restart(void) { intrinsic_di(); mrow = 0; mtick = 0; mplaying = 1; intrinsic_ei(); }
void music_stop(void) { mplaying = 0; }

void snd_init(void) {
  uint8_t r;
  for (r = 0; r < 14; ++r) ay(r, 0);
  ay(7, 0x3F);
  fx_clear(&fxB); fx_clear(&fxC);
  mplaying = 0;
}

// Llamado 50 veces por segundo desde la interrupción.
// mix y npitch son estáticas a propósito: con ellas como variables locales, el optimizador de
// SDCC 4.x descartaba algunas actualizaciones de la máscara (and sin guardar el resultado).
static uint8_t mix, npitch;
void snd_frame(void) {
  uint16_t pa = 0, pb = 0, pc = 0, p;
  uint8_t va = 0, vb = 0, vc = 0, v, fl, nz, m;
  mix = 0x3F; npitch = 0;

  // música
  if (mplaying && music_on) {
    m = mel[mrow];
    if (m) { pa = note_period(m); va = 11 - (mtick >> 1); if (va < 6) va = 6; mix &= ~0x01; }
    m = bass[mrow];
    pb = note_period(m); vb = 9 - mtick; if (vb > 9 || vb < 4) vb = 4;
  }
  if (mplaying && ++mtick >= ROW_FRAMES) { mtick = 0; mrow = (mrow + 1) & 31; }

  // efectos en B (sustituyen al bajo mientras suenan)
  if (fx_tick(&fxB, &p, &v, &fl, &nz)) {
    pb = p; vb = v; mix |= 0x12;
    if (fl & F_TONE) mix &= ~0x02;
    if (fl & F_NOISE) { mix &= ~0x10; npitch = nz; }
  } else if (mplaying && music_on) {
    mix &= ~0x02;
  } else vb = 0;

  // efectos en C
  if (fx_tick(&fxC, &p, &v, &fl, &nz)) {
    pc = p; vc = v;
    if (fl & F_TONE) mix &= ~0x04;
    if (fl & F_NOISE) { mix &= ~0x20; npitch = nz; }
  }

  if (!snd_on) { va = vb = vc = 0; }
  ay(0, pa & 0xFF); ay(1, pa >> 8);
  ay(2, pb & 0xFF); ay(3, pb >> 8);
  ay(4, pc & 0xFF); ay(5, pc >> 8);
  ay(6, npitch & 31);
  ay(7, mix);
  ay(8, va); ay(9, vb); ay(10, vc);
}

// ---------- Efectos ----------
void snd_place(void) {
  intrinsic_di();
  fx_clear(&fxC);
  push(&fxC, 0, 2, 13, 1, F_NOISE, 0, 5);
  push(&fxC, note_period(45), 4, 12, 1, F_TONE, 40, 0);
  intrinsic_ei();
}

void snd_move(void) {
  intrinsic_di();
  if (fxC.head == fxC.tail) push(&fxC, note_period(96), 1, 5, 0, F_TONE, 0, 0);
  intrinsic_ei();
}

static const uint8_t steps[] = { 0, 4, 7, 12, 16, 19, 24 };
void snd_lines(uint8_t n, uint8_t streak) {
  uint8_t k, off = streak > 1 ? streak - 1 : 0, s, base;
  if (off > 4) off = 4;
  intrinsic_di();
  fx_clear(&fxB);
  for (k = 0; k < n; ++k) {
    s = k + off; if (s > 6) s = 6;
    base = 72 + steps[s];
    push_note(&fxB, base, 2, 14, 0);
    push_note(&fxB, base + 4, 2, 13, 0);
    push_note(&fxB, base + 7, 5, 13, 2);
  }
  intrinsic_ei();
}

// Explosión: chasquido de ruido agudo y ruido que se vuelve grave y se apaga en ~1 s.
void snd_boom(void) {
  intrinsic_di();
  fx_clear(&fxC);
  push(&fxC, 0, 2, 15, 0, F_NOISE, 0, 1);
  push(&fxC, 0, 8, 15, 4, F_NOISE, 0, 8);
  push(&fxC, 0, 16, 13, 2, F_NOISE, 0, 18);
  push(&fxC, 0, 26, 9, 3, F_NOISE, 0, 31);
  intrinsic_ei();
}

void snd_fanfare(void) {
  uint8_t i;
  intrinsic_di();
  push_note(&fxB, 72, 4, 13, 2);
  push_note(&fxB, 76, 4, 13, 2);
  push_note(&fxB, 79, 4, 13, 2);
  for (i = 0; i < 4; ++i) { push_note(&fxB, 84, 1, 12, 0); push_note(&fxB, 88, 1, 12, 0); push_note(&fxB, 91, 1, 12, 0); }
  push_note(&fxB, 96, 8, 13, 1);
  intrinsic_ei();
}

void snd_power(void) {
  intrinsic_di();
  push_note(&fxB, 91, 2, 12, 0);
  push_note(&fxB, 96, 5, 12, 1);
  intrinsic_ei();
}

void snd_swap(void) {
  intrinsic_di();
  fx_clear(&fxC);
  push(&fxC, note_period(60), 18, 12, 3, F_TONE | F_NOISE, -12, 3);
  intrinsic_ei();
}

void snd_rotate(void) {
  intrinsic_di();
  fx_clear(&fxC);
  push_note(&fxC, 76, 2, 11, 0);
  push_note(&fxC, 81, 3, 11, 1);
  intrinsic_ei();
}

void snd_error(void) {
  intrinsic_di();
  fx_clear(&fxC);
  push(&fxC, note_period(38), 6, 12, 2, F_TONE | F_VIB, 0, 0);
  intrinsic_ei();
}

void snd_gameover(void) {
  intrinsic_di();
  mplaying = 0;
  fx_clear(&fxB);
  push_note(&fxB, 58, 14, 13, 4); push_rest(&fxB, 2);
  push_note(&fxB, 57, 14, 13, 4); push_rest(&fxB, 2);
  push_note(&fxB, 56, 14, 13, 4); push_rest(&fxB, 2);
  push(&fxB, note_period(55), 50, 13, 5, F_TONE | F_VIB, 0, 0);
  intrinsic_ei();
}
