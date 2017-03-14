#ifndef __DATETIME_H__
#define __DATETIME_H__
typedef struct _DateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} DateTime;

void datetime_update(uint32_t time, DateTime* dt);
#endif
