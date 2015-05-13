/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "chprintf.h"
#include "string.h"

#include "orchard.h"
#include "orchard-shell.h"

#include "charger.h"

void cmd_shipmode(BaseSequentialStream *chp, int argc, char *argv[])
{

  (void)argv;
  (void)argc;
  chprintf(chp, "Battery will disconnect and system will power off\r\n");
  chprintf(chp, "You must disconnect/reconnect power via microUSB to reconnect battery\r\n");

  chargerShipMode();
}

orchard_command("shipmode", cmd_shipmode);

void cmd_boost(BaseSequentialStream *chp, int argc, char *argv[])
{

  (void)argv;
  (void)argc;

  if( argc != 1 ) {
    chprintf(chp, "boost [on|off]\r\n" );
    return;
  }
  chprintf( chp, "wtf: %s\n\r", argv[0] );
  if( strncmp( argv[0], "on", 2 ) == 0 ) {
    chprintf(chp, "Turning boost intent on\r\n");
    chargerBoostIntent(1);
  } else {
    chprintf(chp, "Turning boost intent off\r\n");
    chargerBoostIntent(0);
  }
}

orchard_command("boost", cmd_boost);


void cmd_charge(BaseSequentialStream *chp, int argc, char *argv[])
{

  (void)argv;
  (void)argc;
  
  if( argc != 1 ) {
    chprintf(chp, "charge [on|off]\r\n" );
    return;
  }
  if( strncmp( argv[0], "on", 2 ) == 0 ) {
    chprintf(chp, "Turning charge intent on\r\n");
    chargerChargeIntent(1);
  } else {
    chprintf(chp, "Turning charge intent off\r\n");
    chargerChargeIntent(0);
  }
}

orchard_command("charge", cmd_charge);


void cmd_chgstat(BaseSequentialStream *chp, int argc, char *argv[])
{

  (void)argv;
  (void)argc;
  chargerStat cstat;
  chargerFault cfault;

  cstat = chargerGetStat();
  if( cstat == STAT_FAULT ) {
    cfault = chargerFaultCode();
    switch(cfault) {
    case ENORMAL:
      chprintf(chp, "Charger is normal, but fault code reported. Strange.\r\n");
      break;
      
    case EBOOSTOVP:
      chprintf(chp, "Boost circuit in over voltage protect\r\n");
      break;
      
    case ELOWV:
      chprintf(chp, "Input voltage is too low or boost overcurrent\r\n");
      break;
      
    case ETHERMAL:
      chprintf(chp, "Thermal shutdown\r\n" );
      break;

    case ETIMER:
      chprintf(chp, "Safety or watchdog timer fault\r\n" );
      break;
      
    case EBATTOVP:
      chprintf(chp, "Batterey OVP\r\n" );
      break;
      
    case ENOBATT:
      chprintf(chp, "No battery connected\r\n" );
      break;
      
    default:
      chprintf(chp, "Charger is in an unknown state (program error)\r\n");
    }
  } else {
    switch( cstat ) {
    case STAT_RDY:
      chprintf(chp, "Charger is idle\r\n" );
      break;

    case STAT_CHARGING:
      chprintf(chp, "Charger is charging\r\n" );
      break;

    case STAT_DONE:
      chprintf(chp, "Charger is in charge mode, but finished charging\r\n" );
      break;

    case STAT_FAULT:
      chprintf(chp, "Charge has a fault\r\n" );
      break;

    default:
      chprintf(chp, "Charge is ina n unknown state (program error)\r\n" );
    }
  }
}

orchard_command("chgstat", cmd_chgstat);
