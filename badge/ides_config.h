/*
 * ides_config.h
 * 
 * #defines go here to control app-fight and other badge features 
 *
 * J. Adams 4/2017
 * jna@retina.net
 */

#ifndef __IDES_CONFIG_H__
#define __IDES_CONFIG_H__

/* the maximum level you can reach if you change this you have to
 * update userconfig.c */
#define LEVEL_CAP        10

/* We we will attempt to find a new caesar every these many seconds */
#define CS_ELECT_INT     MS2ST(60000) 

/* You must be this tall to ride this ride */
#define MIN_CAESAR_LEVEL 3           

/* % chance of becoming caesar during election */
#define CAESAR_CHANCE    15

/* The max number of seconds you can assume another form (caesar, bender) */
#define MAX_BUFF_TIME    S2ST(3600)

/* how long to reach full heal */
#define HEAL_TIME        20           /* seconds */

/* We will heal the player at N hp per this interval or 2X HP if
 * UL_PLUSDEF has been activated */
#define HEAL_INTERVAL_US 1000000      /* 1 second */

// production use 66666 uS = 15 FPS. Eeeviil...
// testing use 1000000 (1 sec)
#define FRAME_INTERVAL_US 66666

// WAIT_TIMEs are always in system ticks.
// caveat! the system timer is a uint32_t and can roll over! be aware!
#define DEFAULT_WAIT_TIME MS2ST(10000) // how long we wait for the user to respond. MUST BE IN SYSTEM TICKS. 
#define MAX_ACKWAIT MS2ST(1000)        // if no ACK in 500MS, resend the last packet. MUST BE IN SYSTEM TICKS. 
#define HOLDOFF_MODULO 500             // rand() % HOLDOFF_MODULO + MIN_HOLDOFF = delay between retransmissions
#define MIN_HOLDOFF 50                 // we introduce a small delay if we are resending (contention protocol). MUST BE IN mS
#define MAX_RETRIES 4                  // if we do that 3 times, abort.
#define MOVE_WAIT_TIME MS2ST(10000)    // Max game time. MUST BE IN SYSTEM TICKS. If you do nothing, the game ends.
#endif

