// BLOQTRIX ZX — motor del juego: tablero 8x8, piezas, líneas, casillas especiales y reparto con ayuda.
// No toca el hardware: se puede compilar y medir aparte (ver tools/bench.c).
#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>

#define N 8

// Tipo de casilla
#define K_NONE    0
#define K_BOMB    1
#define K_STONE   2   // piedra entera: la primera línea solo la agrieta
#define K_CRACKED 3   // piedra agrietada: la siguiente línea la rompe

// Colores lógicos (gfx.c los traduce a atributos del Spectrum)
enum {
  C_EMPTY = 0, C_YELLOW, C_CYAN, C_RED, C_GREEN, C_MAGENTA, C_PINK, C_BLUE,
  C_ORANGE, C_TEAL, C_LIME, C_WHITE, C_GREY, C_STONE, C_COUNT
};

typedef struct {
  uint8_t fam, color, h, w, n;
  uint8_t rows[3];              // máscara de bits por fila (bit x = columna x)
  uint8_t cy[9], cx[9], kind[9];
} Piece;

// Tablero real
extern uint8_t occ[N];          // ocupación: una fila por byte
extern uint8_t cellc[N * N];    // color de cada casilla
extern uint8_t cellk[N * N];    // tipo de cada casilla

// Resultado de eliminar líneas
typedef struct {
  uint8_t removed[N], cracked[N], boom[N];   // máscaras por fila
  uint8_t linerows, linecols;                // líneas completas
  uint8_t nlines, bombs, mono, boomcells, clean;
} ClearResult;
extern ClearResult clr;

void engine_init(void);   // llamar una vez al arrancar (prepara tablas)

// Aleatorio (xorshift de 16 bits)
void rng_seed(uint16_t s);
uint8_t rnd8(void);
uint8_t rnd_below(uint8_t n) __z88dk_fastcall;

void board_seed(void);
uint8_t can_place(const uint8_t *b, const Piece *p, int8_t r, int8_t c);
uint8_t fits_anywhere(const uint8_t *b, const Piece *p);
void piece_rotate(Piece *p);
uint8_t piece_rotation_fits(const Piece *p);
void place_piece(const Piece *p, uint8_t r, uint8_t c);
uint8_t full_lines(const uint8_t *b, uint8_t *rows, uint8_t *cols);
void preview_lines(const Piece *p, uint8_t r, uint8_t c, uint8_t *rows, uint8_t *cols);
void resolve_clear(void);

// Reparto: probabilidad de ayuda por ronda (en %), y reparto de 'count' piezas
uint8_t assist_chance(uint8_t round);
uint8_t assist_min_forgiveness(uint8_t round);
void deal_pieces(Piece *out, uint8_t count, uint8_t round, uint8_t assisted);

#endif
