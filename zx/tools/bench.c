// Banco de pruebas del motor: reparte bandejas con ayuda sobre tableros de distinta densidad.
// Se compila para el objetivo +test de z88dk y se ejecuta con z88dk-ticks, que cuenta los ciclos
// de Z80. Ver tools/bench.sh.
#include <stdio.h>
#include <string.h>
#include "../src/engine.h"

#ifndef DENSITY
#define DENSITY 50      // % de casillas ocupadas
#endif
#ifndef DEALS
#define DEALS 20
#endif
#ifndef ROUND
#define ROUND 5
#endif

static Piece tray[3];

static void random_board(uint8_t pct) {
  uint8_t r, c, rows, cols;
  do {
    memset(occ, 0, N); memset(cellc, 0, N * N); memset(cellk, 0, N * N);
    for (r = 0; r < N; ++r)
      for (c = 0; c < N; ++c)
        if (rnd_below(100) < pct) { occ[r] |= 1 << c; cellc[r * N + c] = C_GREY; }
  } while (full_lines(occ, &rows, &cols));
}

int main(void) {
  uint8_t d, i, fits, helpers = 0, nofit = 0;
  engine_init(); rng_seed(1234);
  for (d = 0; d < DEALS; ++d) {
    random_board(DENSITY);
    deal_pieces(tray, 3, ROUND, 1);
    for (i = 0, fits = 0; i < 3; ++i) {
      if (fits_anywhere(occ, &tray[i])) ++fits;
      if (tray[i].fam >= 8) ++helpers;
    }
    if (!fits) ++nofit;
  }
  printf("density %d%% round %d: %d deals, %d helper pieces, %d trays with no fitting piece\n",
         DENSITY, ROUND, DEALS, helpers, nofit);
  return 0;
}
