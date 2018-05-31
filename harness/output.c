#include "harness.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

static int available_width = 0;
static unsigned long output_time = 0;

static void new_line(void)
{
  if (available_width)
  {
    available_width = 0;
    fputs("\"\n", stdout);
  }
}

static void check_time(void)
{
  unsigned long current_time = get_time();

  if (current_time != output_time)
  {
    new_line();
    printf("Time: %ld\n", current_time);
    output_time = current_time;
  }
}

void outputc(char c)
{
  check_time();
  if (!available_width)
  {
    fputs("Output: \"", stdout);
    available_width = 65;
  }
  switch(c)
  {
  case '"':
    fputs("\"", stdout);
    available_width -= 1;
    break;

  case '\\':
    fputs("\\\\", stdout);
    available_width -= 2;
    break;

  case '\n':
    fputs("\\n", stdout);
    available_width -= 2;
    break;

  case '\r':
    fputs("\\r", stdout);
    available_width -= 2;
    break;

  default:
    if (isprint(c))
    {
      available_width -= 1;
      putchar(c);
    }
    else
    {
      available_width -= 4;
      printf("\\%03o", c);
    }
    break;
  }
  if (available_width <= 0)
  {
    available_width = 0;
    fputs("\"\n", stdout);
  }
}

void outputf(const char *format, ...)
{
  va_list args;

  check_time();
  new_line();

  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
