#include "time.h"

#include <stdio.h>

#include "kernel/cpu/hal.h"
#include "kernel/cpu/idt.h"
#include "kernel/memory/vmm.h"

#define PIT_TICKS_PER_SECOND 1000

volatile uint64_t jiffies = 0;

volatile uint64_t boot_seconds, current_seconds;
struct time epoch_time = {
    .year = 1970,
    .month = 1,
    .day = 1,
    .hour = 0,
    .minute = 0,
    .second = 0,
};
struct time current_time;

void set_boot_seconds(uint64_t bs) {
  boot_seconds = bs;
}

void set_current_time(uint16_t year, uint8_t month, uint8_t day,
                      uint8_t hour, uint8_t minute, uint8_t second) {
  current_time.year = year;
  current_time.month = month;
  current_time.day = day;
  current_time.hour = hour;
  current_time.minute = minute;
  current_time.second = second;
  current_seconds = get_seconds(NULL);
}

struct time fat_to_time(uint32_t fat_date, uint32_t fat_time) {
  uint16_t y = (fat_date >> 9) + 1980;
  uint8_t m = (fat_date >> 5) & 0b1111;
  uint8_t d = fat_date & 0b11111;

  uint8_t h = (fat_time >> 12) & 0b1111;
  uint8_t min = (fat_time >> 5) & 0b111111; 
  uint8_t s = (fat_time & 0b11111) << 1; 
  
  struct time t = {
    .year = y,
    .month = m,
    .day = d,
    .hour = h,
    .minute = min,
    .second = s
  };

  return t;
}

// NOTE: MQ 2019-07-25 According to this paper http://howardhinnant.github.io/date_algorithms.html#civil_from_days
static struct time* get_time_from_seconds(int32_t seconds) {
  struct time* t = kcalloc(1, sizeof(struct time));
  int32_t days = seconds / (24 * 3600);

  days += 719468;
  uint32_t era = (days >= 0 ? days : days - 146096) / 146097;
  uint32_t doe = days - era * 146097;                                    // [0, 146096]
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;  // [0, 399]
  uint32_t y = yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);  // [0, 365]
  uint32_t mp = (5 * doy + 2) / 153;                       // [0, 11]
  uint32_t d = doy - (153 * mp + 2) / 5 + 1;               // [1, 31]
  uint32_t m = mp + (mp < 10 ? 3 : -9);                    // [1, 12]

  t->year = y + (m <= 2);
  t->month = m;
  t->day = d;
  t->hour = (seconds % (24 * 3600)) / 3600;
  t->minute = (seconds % (60 * 60)) / 60;
  t->second = seconds % 60;

  return t;
}

// NOTE: MQ 2019-07-25 According to this paper http://howardhinnant.github.io/date_algorithms.html#days_from_civil
static uint32_t get_days(struct time *t) {
  int32_t year = t->year;
  uint32_t month = t->month;
  uint32_t day = t->day;

  year -= month <= 2;

  uint32_t era = (year >= 0 ? year : year - 399) / 400;
  uint32_t yoe = (year - era * 400);                                        // [0, 399]
  uint32_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;  // [0, 365]
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                     // [0, 146096]
  return era * 146097 + (doe)-719468;
}

uint32_t get_seconds(struct time *t) {
  if (t == NULL)
    t = &current_time;

  return get_days(t) * 24 * 3600 + t->hour * 3600 + t->minute * 60 + t->second;
}

/*
uint64_t get_milliseconds(struct time *t) {
  if (t == NULL)
    return boot_seconds * 1000 + jiffies;
  else
    return get_seconds(t) * 1000 + jiffies % 1000;
}
*/

struct time *get_time(int32_t seconds) {
  return seconds == 0 ? &current_time : get_time_from_seconds(seconds);
}

/*
uint64_t get_milliseconds_since_epoch() {
  return get_milliseconds(NULL) - get_milliseconds(&epoch_time);
}
*/

static uint32_t tick = 0;

static void timer_create(uint32_t frequency) {
  // Firstly, register our timer callback.
  // register_interrupt_handler(IRQ0, &timer_callback);

  // The value we send to the PIT is the value to divide it's input clock
  // (1193180 Hz) by, to get our required frequency. Important to note is
  // that the divisor must be small enough to fit into 16-bits.
  uint16_t divisor = 1193180 / frequency;

  // Send the command byte.
  outportb(0x43, 0x36);

  // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
  uint8_t l = (uint8_t)(divisor & 0xFF);
  uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

  // Send the frequency divisor.
  outportb(0x40, l);
  outportb(0x40, h);
}
