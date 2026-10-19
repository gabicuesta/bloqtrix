// Genera src/notes.h: tablas de notas (MIDI 36..96) para el beeper del 48K y el chip AY del 128K.
// Uso: node tools/gen_notes.js
const fs = require('fs');
const path = require('path');

const FIRST = 36, LAST = 96;
const freq = m => 440 * Math.pow(2, (m - 69) / 12);

// Beeper: medio periodo = 26 * bucles + 68 ciclos a 3,5 MHz (ver beep_raw en sound48.c)
const beepLoops = m => Math.max(1, Math.round((3500000 / (2 * freq(m)) - 68) / 26));
// AY del 128K: reloj de 1,7734 MHz, periodo de tono = reloj / (16 * f)
const ayPeriod = m => Math.round(1773400 / (16 * freq(m)));

const rows = [];
for (let m = FIRST; m <= LAST; m++) rows.push(m);
const fmt = (fn) => rows.map(fn).join(', ');

const out = `// Generado por tools/gen_notes.js — no editar a mano.
// Notas MIDI ${FIRST}..${LAST} (Do2..Do7). NOTE(m) = índice en las tablas.
#ifndef NOTES_H
#define NOTES_H
#include <stdint.h>
#define NOTE_FIRST ${FIRST}
#define NOTE(m) ((m) - NOTE_FIRST)
static const uint16_t note_freq[] = { ${fmt(m => Math.round(freq(m)))} };
#ifdef MODEL128
static const uint16_t note_ay[] = { ${fmt(ayPeriod)} };
#else
static const uint16_t note_beep[] = { ${fmt(beepLoops)} };
#endif
#endif
`;
fs.writeFileSync(path.join(__dirname, '..', 'src', 'notes.h'), out);
console.log('src/notes.h escrito');
