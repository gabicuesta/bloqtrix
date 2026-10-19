// BLOQTRIX ZX — sonido. Misma interfaz para las dos variantes:
//  - 48K (sound48.c): beeper de 1 bit; cada efecto se toca entero antes de seguir.
//  - 128K (sound128.c): chip AY de 3 canales; los efectos se ponen en cola y suenan
//    desde la interrupción de 50 Hz mientras el juego sigue, con música de fondo.
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

extern uint8_t snd_on;      // tecla S: todo el sonido
extern uint8_t music_on;    // tecla M (solo 128K)

void snd_init(void);
void snd_frame(void);       // desde la interrupción
void snd_place(void);
void snd_lines(uint8_t n, uint8_t streak);
void snd_boom(void);
void snd_fanfare(void);
void snd_power(void);
void snd_swap(void);
void snd_rotate(void);
void snd_error(void);
void snd_gameover(void);
void snd_move(void);
void music_restart(void);
void music_stop(void);

#endif
