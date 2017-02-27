#ifndef __UNLOCKS_H__
#define __UNLOCKS_H__

/* these are a set of bit flags in config->unlocks which can do different things to the badge */

/* You're an EFF donor, enables EFF logo on badge screen and +10% def*/
#define UL_PLUSDEF      ( 1 << 0 ) // EFF + 10% DEF
#define UL_PLUSHP       ( 1 << 1 ) // +10% HP
#define UL_PLUSMIGHT    ( 1 << 2 ) // +1 MIGHT
#define UL_LUCK         ( 1 << 3 ) // +20% LUCK
#define UL_HEAL         ( 1 << 4 ) // 2X HEAL
#define UL_LEDS         ( 1 << 5 ) // MORE LEDs
#define UL_CAESAR       ( 1 << 6 ) // CAESAR
#define UL_SENATOR      ( 1 << 7 ) // SENATOR
#define UL_TIMELORD     ( 1 << 8 ) // TIMELORD
#endif
