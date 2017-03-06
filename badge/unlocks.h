#ifndef __UNLOCKS_H__
#define __UNLOCKS_H__

/* these are a set of bit flags in config->unlocks which can do different things to the badge */

/*                                                   Implemented? */
#define UL_PLUSDEF      ( 1 << 0 ) // EFF + 10% DEF  y
#define UL_PLUSHP       ( 1 << 1 ) // +10% HP        y
#define UL_PLUSMIGHT    ( 1 << 2 ) // +1 MIGHT       y
#define UL_LUCK         ( 1 << 3 ) // +20% LUCK      y
#define UL_HEAL         ( 1 << 4 ) // 2X HEAL        y
#define UL_LEDS         ( 1 << 5 ) // MORE LEDs      n
#define UL_CAESAR       ( 1 << 6 ) // CAESAR         n
#define UL_SENATOR      ( 1 << 7 ) // SENATOR        n
#define UL_TIMELORD     ( 1 << 8 ) // TIMELORD       n
#endif
