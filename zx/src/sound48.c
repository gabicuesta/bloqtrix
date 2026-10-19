// BLOQTRIX ZX — sonido del 48K por el beeper (bit 4 del puerto 0xFE).
// Dos rutinas en ensamblador: un tono cuadrado de periodo exacto y un ruido blanco con
// un registro de desplazamiento (LFSR). Todo lo demás se compone con ellas.
// Mientras suena se desactivan las interrupciones para que el tono no se entrecorte.
#include <intrinsic.h>
#include "sound.h"
#include "notes.h"

uint8_t snd_on = 1;
uint8_t music_on = 0;

uint16_t bp_period, bp_count, nseed = 0x5A3C;
uint8_t bp_delay;

// Medio periodo = 26 * bp_period + 67 ciclos (ver tools/gen_notes.js); bp_count medios periodos.
static void beep_raw(void) __naked {
  __asm
    ld hl,(_bp_count)
    ld a,h
    or l
    ret z
    ld e,0
  br_loop:
    ld a,e
    xor 0x10
    ld e,a
    out (0xfe),a
    ld bc,(_bp_period)
  br_wait:
    dec bc
    ld a,b
    or c
    jr nz,br_wait
    dec hl
    ld a,h
    or l
    jr nz,br_loop
    ret
  __endasm;
}

// Ruido: en cada paso el LFSR decide si el altavoz cambia; bp_delay alarga cada paso
// (más retardo = ruido más grave). Unos 80 + 13 * bp_delay ciclos por paso.
static void noise_raw(void) __naked {
  __asm
    ld hl,(_nseed)
    ld de,(_bp_count)
    ld a,d
    or e
    ret z
    ld c,0
  nr_loop:
    add hl,hl
    jr nc,nr_skip
    ld a,l
    xor 0x2D
    ld l,a
    ld a,c
    xor 0x10
    ld c,a
    out (0xfe),a
  nr_skip:
    ld a,(_bp_delay)
    ld b,a
  nr_wait:
    djnz nr_wait
    dec de
    ld a,d
    or e
    jr nz,nr_loop
    ld (_nseed),hl
    ret
  __endasm;
}

static void tone(uint16_t loops, uint16_t halfcycles) {
  if (!snd_on) return;
  bp_period = loops; bp_count = halfcycles;
  intrinsic_di(); beep_raw(); intrinsic_ei();
}

// Nota MIDI m durante ms milisegundos
static void note(uint8_t m, uint16_t ms) {
  uint8_t i;
  if (m > 96) m = 96;
  if (m < NOTE_FIRST) m = NOTE_FIRST;
  i = NOTE(m);
  tone(note_beep[i], (uint16_t)((uint32_t)note_freq[i] * ms / 500));
}

// Ruido durante ~ms milisegundos con el retardo dado
static void noise(uint8_t delay, uint16_t ms) {
  if (!snd_on) return;
  bp_delay = delay ? delay : 1;
  bp_count = (uint16_t)((uint32_t)3500 * ms / (80 + 13 * (uint16_t)bp_delay));
  intrinsic_di(); noise_raw(); intrinsic_ei();
}

static void rest(uint8_t frames) { while (frames--) intrinsic_halt(); }

void snd_init(void) {}
void snd_frame(void) {}
void snd_move(void) {}
void music_restart(void) {}
void music_stop(void) {}

// Golpe seco al encajar: chasquido + tono grave que cae
void snd_place(void) {
  noise(3, 4);
  note(45, 14);
  note(40, 16);
}

// Arpegio por línea; con varias líneas, cada una más aguda; la racha sube todo el tono.
static const uint8_t steps[] = { 0, 4, 7, 12, 16, 19, 24 };
void snd_lines(uint8_t n, uint8_t streak) {
  uint8_t k, off = streak > 1 ? streak - 1 : 0, s, base;
  if (off > 4) off = 4;
  for (k = 0; k < n; ++k) {
    s = k + off; if (s > 6) s = 6;
    base = 72 + steps[s];
    note(base, 32); note(base + 4, 32); note(base + 7, 45);
  }
}

// Explosión: ruido que empieza agudo y denso y se vuelve grave (retardos cada vez mayores).
void snd_boom(void) {
  uint8_t i;
  noise(1, 18);
  for (i = 0; i < 14; ++i) noise(4 + i * 6, 30);
}

void snd_fanfare(void) {
  uint8_t i;
  note(72, 60); note(76, 60); note(79, 60);
  for (i = 0; i < 3; ++i) { note(84, 16); note(88, 16); note(91, 16); }
  note(96, 70);
}

void snd_power(void) { note(91, 35); note(96, 60); }

void snd_swap(void) {
  uint8_t m;
  for (m = 60; m <= 96; m += 3) note(m, 12);
}

void snd_rotate(void) { note(76, 14); note(81, 18); }

void snd_error(void) { note(38, 70); }

// "Wah wah wah waaah": tres notas que bajan por semitonos y una última larga con vibrato.
void snd_gameover(void) {
  uint8_t i;
  uint16_t base = note_beep[NOTE(55)];
  note(58, 300); rest(2);
  note(57, 300); rest(2);
  note(56, 300); rest(2);
  for (i = 0; i < 12; ++i)
    tone(i & 1 ? base + 4 : base - 4, (uint16_t)((uint32_t)note_freq[NOTE(55)] * 75 / 500));
}
