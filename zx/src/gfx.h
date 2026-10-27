// BLOQTRIX ZX — gráficos: memoria de pantalla, atributos, texto y dibujos de las casillas.
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

// Atributos: FLASH | BRIGHT | papel << 3 | tinta
#define A_FLASH  0x80
#define A_BRIGHT 0x40
#define INK(c)   (c)
#define PAPER(c) ((c) << 3)
enum { Z_BLACK, Z_BLUE, Z_RED, Z_MAGENTA, Z_GREEN, Z_CYAN, Z_YELLOW, Z_WHITE };

// Caracteres propios (en las cadenas, como escapes octales)
#define CH_E_ACUTE "\200"
#define CH_I_ACUTE "\201"
#define CH_O_ACUTE "\202"
#define CH_A_ACUTE "\203"
#define CH_U_ACUTE "\204"
#define CH_N_TILDE "\205"
#define CH_EXCL    "\206"   // ¡
#define CH_QUEST   "\207"   // ¿
#define CH_COPY    "\210"   // ©

extern uint8_t bmp_block[32], bmp_empty[32], bmp_ghost[32], bmp_stone[32], bmp_cracked[32];
extern uint8_t bmp_bomb[2][32];
extern uint8_t bmp8_block[8], bmp8_ghost[8], bmp8_bomb[8], bmp8_stone[8];

extern const uint8_t color_paper_attr[];   // casilla llena: papel del color, tinta negra
extern const uint8_t color_ink_attr[];     // sombra: tinta del color sobre negro

void gfx_init(void);
void gfx_cls(void);
uint8_t *scr_addr(uint8_t row, uint8_t col);
void set_attr(uint8_t row, uint8_t col, uint8_t a);
void fill_attr(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t a);
void clear_area(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t a);
void draw8(uint8_t row, uint8_t col, const uint8_t *bmp, uint8_t a);
void draw16(uint8_t row, uint8_t col, const uint8_t *bmp, uint8_t a);
void attr16(uint8_t row, uint8_t col, uint8_t a);
void print_at(uint8_t row, uint8_t col, const char *s, uint8_t a);
void print_tall(uint8_t row, uint8_t col, const char *s, uint8_t a);   // doble alto
void print_huge(uint8_t row, uint8_t col, const char *s, uint8_t a);   // doble alto y ancho
void draw_frame(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t a);
uint8_t text_len(const char *s);

#endif
