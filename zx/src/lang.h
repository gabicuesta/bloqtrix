// BLOQTRIX ZX — textos en español e inglés. Se elige en el título con la tecla L.
// TX(id) devuelve el texto en el idioma activo.
#ifndef LANG_H
#define LANG_H

#include "gfx.h"

enum {
  S_SCORE, S_BEST, S_STREAK, S_SWAP, S_ROTATE, S_MUSIC, S_ON, S_OFF,
  S_HINT, S_STUCK, S_DEALING, S_DOUBLE, S_TRIPLE, S_LINES_SUFFIX,
  S_BOOM, S_COLOUR, S_CLEAN, S_PLUS_SWAP, S_PLUS_ROT,
  S_NOROOM, S_NEWBEST, S_ST_LINES, S_ST_STREAK, S_ST_ROUNDS, S_ST_PIECES, S_ST_BOMBS, S_ST_CLEAN, S_AGAIN,
  S_SUBTITLE, S_MOVE_KEYS, S_MOVE, S_PICK, S_PLACE_KEYS, S_PLACE, S_SOUND, S_MUSIC_COLON, S_FIRE,
  S_LANGUAGE, S_RULE1, S_RULE2, S_PRESS,
  S_COUNT
};

static const char *const txt[S_COUNT][2] = {
  { "PUNTOS", "SCORE" },
  { "R" CH_E_ACUTE "CORD", "BEST" },
  { "RACHA", "STREAK" },
  { "CAMBIAR", "SWAP" },
  { "GIRAR", "ROTATE" },
  { "M" CH_U_ACUTE "SICA", "MUSIC" },
  { "S" CH_I_ACUTE " ", "ON " },
  { "NO ", "OFF" },
  { "QAOP/CURSORES 1-3 ESPACIO C G S", "QAOP/CURSORS 1-3 SPACE C G S" },
  { "SIN HUECO: C O G", "NO ROOM: C OR G" },
  { "REPARTIENDO...", "DEALING..." },
  { " DOBLE", " DOUBLE" },
  { " TRIPLE", " TRIPLE" },
  { " L" CH_I_ACUTE "NEAS", " LINES" },
  { CH_EXCL "BUM! +", "BOOM! +" },
  { "L" CH_I_ACUTE "NEA DE COLOR +", "COLOUR LINE +" },
  { CH_EXCL "TABLERO LIMPIO! +300", "CLEAN BOARD! +300" },
  { "+1 CAMBIAR", "+1 SWAP" },
  { "+1 GIRAR", "+1 ROTATE" },
  { "SIN HUECO", " NO ROOM " },
  { CH_EXCL "NUEVO R" CH_E_ACUTE "CORD!", "NEW BEST SCORE!" },
  { "L" CH_I_ACUTE "NEAS", "LINES" },
  { "MEJOR RACHA", "BEST STREAK" },
  { "RONDAS", "ROUNDS" },
  { "PIEZAS", "PIECES" },
  { "BOMBAS", "BOMBS" },
  { "LIMPIOS", "CLEAN" },
  { "ESPACIO: OTRA", "SPACE: AGAIN" },
  { "UN PUZLE DE BLOQUES 8X8", "AN 8X8 BLOCK PUZZLE" },
  { "QAOP / CURSORES", "QAOP / CURSORS" },
  { "MOVER", "MOVE" },
  { "ELEGIR PIEZA", "PICK PIECE" },
  { "ESPACIO / 0", "SPACE / 0" },
  { "COLOCAR", "PLACE" },
  { "SONIDO", "SOUND" },
  { "M" CH_U_ACUTE "SICA:", "MUSIC:" },
  { "DISPARO", "FIRE" },
  { "ESPA" CH_N_TILDE "OL", "ENGLISH" },
  { "L" CH_I_ACUTE "NEAS SEGUIDAS: RACHA HASTA x8", "CHAIN LINES: STREAK UP TO x8" },
  { "BOMBA: ARRASA 3X3  PIEDRA: 2", "BOMB CLEARS 3X3  STONE TAKES 2" },
  { "PULSA ESPACIO", "PRESS SPACE" },
};

extern uint8_t lang;   // 0 = español, 1 = inglés
#define TX(id) (txt[id][lang])

#endif
