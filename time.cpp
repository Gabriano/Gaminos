// file: "time.cpp"

// Copyright (c) 2001 by Marc Feeley and Université de Montréal, All
// Rights Reserved.
//
// Revision History
// 22 Sep 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "time.h"
#include "asm.h"
#include "apic.h"
#include "intr.h"
#include "rtc.h"
#include "term.h"

//-----------------------------------------------------------------------------

// Rational arithmetic routines.

#ifdef USE_TSC_FOR_TIME
#ifdef USE_APIC_FOR_TIMER

static rational make_rational (uint64 num, uint64 den)
{
  rational result;
  uint64 x = num;
  uint64 y = den;

  while (y != 0)
    {
      int64 t = x;
      x = y;
      y = t % y;
    }

  result.num = num / x;
  result.den = den / x;

  return result;
}

static rational rational_floor (rational x)
{
  rational result;

  result.num = x.num / x.den;
  result.den = 1;

  return result;
}

static rational rational_inverse (rational x)
{
  return make_rational (x.den, x.num);
}

static bool rational_less_than (rational x, rational y)
{
  return CAST(uint64,x.num) * y.den < CAST(uint64,y.num) * x.den;
}

static rational rational_add (rational x, rational y)
{
  return make_rational
           (CAST(uint64,x.num) * y.den + CAST(uint64,y.num) * x.den,
            CAST(uint64,x.den) * y.den);
}

static rational rational_subtract (rational x, rational y)
{
  return make_rational
           (CAST(uint64,x.num) * y.den - CAST(uint64,y.num) * x.den,
            CAST(uint64,x.den) * y.den);
}

static rational rational_rationalize2 (rational x, rational y)
{
  rational fx = rational_floor (x);
  rational fy = rational_floor (y);

  if (!rational_less_than (fx, x))
    return fx;

  if (rational_less_than (fx, fy) || rational_less_than (fy, fx))
    return rational_add (fx, make_rational (1, 1));

  return rational_add
           (fx,
            rational_inverse
              (rational_rationalize2
                 (rational_inverse (rational_subtract (y, fy)),
                  rational_inverse (rational_subtract (x, fx)))));
}

static rational rational_rationalize (rational x, rational y)
{
  if (rational_less_than (y, x))
    {
      rational t;
      t = y;
      y = x;
      x = t;
    }

  rational diff = rational_subtract (y, x);
  rational sum = rational_add (y, x);

  if (rational_less_than (diff, sum))
    return rational_rationalize2 (diff, sum);

  return diff;
}

#endif
#endif

//-----------------------------------------------------------------------------

#ifdef USE_IRQ8_FOR_TIME

volatile uint64 _irq8_counter = 0;
time pos_infinity = { 18446744073709551615ULL };
time neg_infinity = { 0 };

void irq8 ()
{
  ACKNOWLEDGE_IRQ(8);

  _irq8_counter++;

  outb (RTC_REGC, RTC_PORT_ADDR); // must also read register C to
  inb (RTC_PORT_DATA);            // acknowledge RTC interrupt
}

#endif

#ifdef USE_TSC_FOR_TIME

static uint64 tsc_at_refpoint = 0;
uint32 _tsc_counts_per_sec; // NOTE: works up to a 4.2 GHz processor clock
time pos_infinity = { 18446744073709551615ULL };
time neg_infinity = { 0 };

#ifdef USE_APIC_FOR_TIMER
rational _cpu_bus_multiplier;
#endif

#endif

void setup_time ()
{
  // It is assumed that interrupts are currently disabled.  They might
  // affect timing accuracy.

#ifdef USE_IRQ8_FOR_TIME

#if IRQ8_COUNTS_PER_SEC == 2
#define RTC_RATE RTC_REGA_2HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 4
#define RTC_RATE RTC_REGA_4HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 8
#define RTC_RATE RTC_REGA_8HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 16
#define RTC_RATE RTC_REGA_16HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 32
#define RTC_RATE RTC_REGA_32HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 64
#define RTC_RATE RTC_REGA_64HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 128
#define RTC_RATE RTC_REGA_128HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 256
#define RTC_RATE RTC_REGA_256HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 512
#define RTC_RATE RTC_REGA_512HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 1024
#define RTC_RATE RTC_REGA_1024HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 2048
#define RTC_RATE RTC_REGA_2048HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 4096
#define RTC_RATE RTC_REGA_4096HZ
#endif
#if IRQ8_COUNTS_PER_SEC == 8192
#define RTC_RATE RTC_REGA_8192HZ
#endif

  outb (RTC_REGA, RTC_PORT_ADDR);
  outb (RTC_REGA_OSC_ON | RTC_RATE, RTC_PORT_DATA);

  outb (RTC_REGB, RTC_PORT_ADDR);
  outb (RTC_REGB_PIE | RTC_REGB_DM_BCD | RTC_REGB_24, RTC_PORT_DATA);

#endif

#ifdef USE_IRQ8_FOR_TIME

  int samples_left = 2;

#endif

#ifdef USE_TSC_FOR_TIME

  int samples_left = 3;
  uint64 old_tsc = 0;

#ifdef USE_APIC_FOR_TIMER

  uint32 old_apic_timer_count = 0;

  APIC_INITIAL_TIMER_COUNT = 0xffffffff;

#endif
#endif

  uint8 old_sec = 255;

  for (;;)
    {
      outb (RTC_REGA, RTC_PORT_ADDR);
      if ((inb (RTC_PORT_DATA) & RTC_REGA_UIP) == 0)
        {
          outb (RTC_SEC, RTC_PORT_ADDR);
          uint8 new_sec = inb (RTC_PORT_DATA);

          if (old_sec != new_sec)
            {
#ifdef USE_TSC_FOR_TIME
              uint64 new_tsc = rdtsc ();
#ifdef USE_APIC_FOR_TIMER
              uint32 new_apic_timer_count = APIC_CURRENT_TIMER_COUNT;
#endif
#endif

              if (--samples_left == 0)
                {
#ifdef USE_TSC_FOR_TIME
                  tsc_at_refpoint = new_tsc;
                  _tsc_counts_per_sec = new_tsc - old_tsc;
#ifdef USE_APIC_FOR_TIMER
                  _cpu_bus_multiplier
                    = rational_rationalize
                        (make_rational (_tsc_counts_per_sec >> 10,
                                        (old_apic_timer_count
                                         - new_apic_timer_count) >> 10),
                         make_rational (1,
                                        16));
#endif
#endif
                  break;
                }

              old_sec = new_sec;

#ifdef USE_TSC_FOR_TIME
              old_tsc = new_tsc;
#ifdef USE_APIC_FOR_TIMER
              old_apic_timer_count = new_apic_timer_count;
#endif
#endif
            }
        }
    }

#ifdef USE_IRQ8_FOR_TIME
  _irq8_counter = 0;
  ENABLE_IRQ(8);
#endif
}

//-----------------------------------------------------------------------------

// Local Variables: //
// mode: C++ //
// End: //
