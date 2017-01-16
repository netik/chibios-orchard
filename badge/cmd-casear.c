/*
 * cmd-casear.c: implement the terribly awful, no good casear cipher.
 * rot13: we has it. 
 *
 * J. Adams <jna@retina.net> 1/13/2017
 */ 

#define STANDALONE
#define GET_MACRO(_1,_2,_3,NAME,...) NAME
#define FOO2(x,y) printf(y)
#define FOO3(x,y,z) printf(y,z)
#define chprintf(...) GET_MACRO(__VA_ARGS__, FOO3, FOO2)(__VA_ARGS__)

#ifndef STANDALONE
#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-app.h"

#define uint8_t unsigned int
#else
#include <stdio.h>
#include <ctype.h>
#endif

#include <stdlib.h>
#include <string.h>

static char *casearShift(int8_t shift_size, char *str) {
  uint8_t i;
  static char output[255];
  memset(&output, 0, 255);
  
  for(i = 0; i < strlen(str); i++) {
    if ((str[i] >= 65) && (str[i] <= 90))
      output[i] = (((str[i] - 65 + shift_size) % 26) + 65);
    // Lowercase letters
    else if ((str[i] >= 97) && (str[i] <= 122))
      output[i] = (((str[i] - 97 + shift_size) % 26) + 97);
    else
      output[i] = str[i];

    // stop buffer overflows
    if (i == 254) { return(output); }
  }
  return(output);
}


#ifdef STANDALONE
int main(int argc, char *argv[])
#else
static void cmd_casear(BaseSequentialStream *chp, int argc, char *argv[])
#endif
{
  int8_t shift_size;
  
  (void)argv;
  (void)argc;


  if (argc < 2) {
    chprintf(chp, "Casear:\r\n");
    chprintf(chp, "  Usage: casear +/-shift message\r\n");
    chprintf(chp, "  An implementation of the casear shift cipher,\r\n");
    chprintf(chp, "  a terrible, no good, weak form of cryptography. shift=13=rot13 :) )\r\n");
    return 0;
  } else {
    shift_size = atoi(argv[1]);
    if (shift_size == 0 || shift_size > 26) {
      chprintf(chp, "Invalid shift size.\r\n");
      return 0;
    }

  }
  
  // test case: 
  // ./a.out 13 ABCDEFGHIHJKLMNOPQRSTUVWXYZ
  // NOPQRSTUVUWXYZABCDEFGHIJKLM
  //
  // FOO BAR BAZ
  // SBB ONE ONM rot 13

  printf("%s\r\n", casearShift(shift_size, argv[2]));
  return 0;
}

#ifndef STANDALONE
orchard_command("enemylist", cmd_enemylist);
#endif

