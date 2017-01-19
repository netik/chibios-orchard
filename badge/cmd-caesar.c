/*
 * cmd-casear.c: implement the terribly awful, no good casear cipher.
 * rot13: we has it. 
 *
 * J. Adams <jna@retina.net> 1/13/2017
 */ 

#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-app.h"

#include <stdlib.h>
#include <string.h>

static char *caesarShift(int8_t shift_size, char *str) {
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


static void cmd_caesar(BaseSequentialStream *chp, int argc, char *argv[])
{
  int8_t shift_size;
  
  (void)argv;
  (void)argc;


  if (argc < 2) {
    chprintf(chp, "Caesar:\r\n");
    chprintf(chp, "  Usage: caesar N message\r\n");
    chprintf(chp, "  An implementation of the Caesar shift cipher,\r\n");
    chprintf(chp, "  a terrible, no good, weak form of cryptography. N=13=rot13 :) )\r\n");
    chprintf(chp, "  message should not contain spaces.\r\n");
    return;
  } else {
    shift_size = atoi(argv[0]);
    if (shift_size == 0 || shift_size > 26) {
      chprintf(chp, "Invalid shift size.\r\n");
      return;
    }

  }
  
  // test case: 
  // ./a.out 13 ABCDEFGHIHJKLMNOPQRSTUVWXYZ
  // NOPQRSTUVUWXYZABCDEFGHIJKLM
  //
  // FOO BAR BAZ
  // SBB ONE ONM rot 13

  chprintf(chp, "%s\r\n", caesarShift(shift_size, argv[1]));
  return;
}

orchard_command("caesar", cmd_caesar);

