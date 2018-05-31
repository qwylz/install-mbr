#include "harness.h"

static unsigned long current_time = 0;
static unsigned long current_rate = 1;
static unsigned long current_step = 0;

void init_time(unsigned long t)
{
  current_time = t;
}

void set_rate(unsigned long rate)
{
  current_rate = rate;
}

unsigned long get_time(void)
{
  return current_time;
}

void tick()
{
  if (++current_step >= current_rate)
  {
    ++current_time;
    process_events_at(current_time);
    current_step = 0;
  }
}

void warp_time(unsigned long t)
{
  outputf("Warped time to %ld\n", t);
  process_events_at(t);
  current_time = t;
  current_step = 0;
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
