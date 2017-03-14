// naive implementation of seconds to datetime. 
#include "ch.h"
#include "hal.h"
#include "datetime.h"

static uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void datetime_update(uint32_t time, DateTime* dt) {
  uint32_t days = (time / 86400);

  dt->year = 1970;
  dt->month = 1;
  dt->day = 1;

  while(1) { // rewrite this!
    char leap_year = (dt->year % 4) == 0;

    for(int i = 0; i < 12 ;i++) {
      uint8_t days_in_month = monthDays[i];

      if(i == 1 && leap_year) { // February of leap year
        days_in_month++;
      }

      if(days < days_in_month) {
        dt->day = days + 1;
        days = 0;
        break;
      }

      days -= days_in_month;

      dt->month++;
    }

    if(days == 0) {
      break;
    }

    dt->month = 1;
    dt->year++;
  }

  uint32_t seconds = (time % 86400);

  dt->hour = seconds / 3600;
  dt->minute = (seconds % 3600) / 60;
  dt->second = (seconds % 3600) % 60;
}
