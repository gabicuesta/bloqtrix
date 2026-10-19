// Prueba del motor: compara fits_anywhere (ensamblador con tabla) con una comprobación
// casilla a casilla con can_place, en tableros aleatorios y para todas las piezas y giros.
#include <stdio.h>
#include <string.h>
#include "../src/engine.h"

static Piece p;
static uint8_t brute(const uint8_t *b, const Piece *q) {
  int8_t r, c;
  for (r = 0; r + q->h <= N; ++r)
    for (c = 0; c + q->w <= N; ++c)
      if (can_place(b, q, r, c)) return 1;
  return 0;
}

int main(void) {
  uint16_t t, bad = 0, checks = 0;
  uint8_t r, c, k, rot, pct;
  engine_init(); rng_seed(4321);
  for (t = 0; t < 400; ++t) {
    pct = 30 + rnd_below(65);
    for (r = 0; r < N; ++r) { occ[r] = 0; for (c = 0; c < N; ++c) if (rnd_below(100) < pct) occ[r] |= 1 << c; }
    for (k = 0; k < 6; ++k) {
      deal_pieces(&p, 1, 1, 0);
      for (rot = 0; rot < 4; ++rot) {
        ++checks;
        if ((fits_anywhere(occ, &p) != 0) != brute(occ, &p)) {
          if (bad < 5) printf("MISMATCH fam %d h %d w %d rows %d %d %d asm %d brute %d\n", p.fam, p.h, p.w, p.rows[0], p.rows[1], p.rows[2], fits_anywhere(occ, &p), brute(occ, &p));
          ++bad;
        }
        piece_rotate(&p);
      }
    }
  }
  printf("%u checks, %u mismatches\n", checks, bad);
  return 0;
}
