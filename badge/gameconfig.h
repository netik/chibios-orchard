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
/* the max number of seconds you can assume another form */
#define MAX_BUFF_TIME    S2ST(3600)

/* we we will attempt to find a new caesar every these many seconds */
#define CS_ELECT_INT     MS2ST(10000) /* 10 sec */

/* how long to reach full heal */
#define HEAL_TIME        25           /* seconds */

/* we will heal the player at N hp per this interval or 2X HP if
 *  UL_PLUSDEF has been activated */

#define HEAL_INTERVAL_US 1000000      /* 1 second */

/* the maximum level you can reach if you change this you have to
 * update userconfig.c */

#define LEVEL_CAP      10

#endif

