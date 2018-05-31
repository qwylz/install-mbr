#include "harness.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned long parse_ul(const char *name, const char *arg)
{
  char *end;
  unsigned long result = strtoul(arg, &end, 0);
  if (*end || !*arg)
  {
    fprintf(stderr, "Invalid %s: %s\n", name, arg);
    exit(1);
  }
  return result;
}

void args_init(int argc, char *argv[])
{
  unsigned int shift_state = 0;
  while(1)
  {
    int opt = getopt(argc, argv, "a:d:D:f:i:k:r:t:w:");
    if (opt == -1)
      break;
    switch(opt)
    {
    case 'a':
      set_cpu_limit(parse_ul("CPU limit", optarg));
      break;
      
    case 'd': /* Debugging. */
      while (*optarg)
      {
	switch(*optarg++)
	{
	case 'v':
	  vm86_debug();
	  break;
	case 's':
	  vm86_step();
	  break;
	case 'e':
	  event_debug();
	  break;
	}
      }
      break;

    case 'D': /* Set date in Y-M-D format. */
      {
	char *next;
	unsigned long year = strtoul(optarg, &next, 0);
	unsigned long month = strtoul(*next-'-'?"0":next+1, &next, 0);
	unsigned long day = strtoul(*next-'-'?"0":next+1, &next, 0);
	if (year-1970>129 || month-1>11 || *next ||
	    day-1>27+(month^2?2+((month>>3)^(month&1)):!(year&3)))
	{
	  fprintf(stderr, "Failed to parse date or out of range: %s\n"
		  "Use format: YYYY-MM-DD.  Year must be 1970-2099.\n",
		  optarg);
	  exit(1);
	}
	set_date(year, month, day);
      }
      break;

    case 'f':
      set_mbr(optarg);
      break;

    case 'i':
      {
	unsigned int if_enable = 0;
	unsigned int if_mask = -1;
	int enable = 1;
	char *p = optarg;
	while (*p)
	{
	  switch (*p++)
	  {
	  case '+': enable = 1; break;
	  case '-': enable = 0; break;
	  case 'b':
	    if_mask &= ~INTERFACE_BIG_DISK;
	    if_enable = enable
	      ? if_enable|INTERFACE_BIG_DISK
	      : if_enable&~INTERFACE_BIG_DISK;
	    break;
	  default:
	    fprintf(stderr, "Unknown interface: '%c'\n", p[-1]);
	    exit(1);
	  }
	}
	set_interfaces(if_enable, if_mask);
      }
      break;
      
    case 'k':
      {
	char *arg = optarg;
	if(arg[0] == '+' || arg[0] == '-' || arg[0] == '=')
	{
	  int add = arg[0] != '-';
	  unsigned int mask = 0;
	  if (arg[0] == '=')
	    shift_state = 0;
	  while(*++arg)
	  {
	    switch(*arg)
	    {
	    case 'r':
	      mask |= 1;
	      break;
	    case 'l':
	      mask |= 2;
	      break;
	    case 'c':
	      mask |= 4;
	      break;
	    default:
	      fprintf(stderr, "Unknown shift character: '%c'.\n",
		      *arg);
	      exit(1);
	    }
	  }
	  shift_state = add ? shift_state|mask : shift_state&~mask;
	  put_shift_event(shift_state);
	}
	else
	  put_key_event(parse_ul("keycode", arg));
      }
      break;
      
    case 'r':
      put_rate_event(parse_ul("tick rate", optarg));
      break;
      
    case 't':
      init_time(parse_ul("initial time", optarg));
      break;
      
    case 'w':
      put_wait_event(parse_ul("wait", optarg));
      break;
      
    case '?':
      exit(1);
    }
  }
  for(;optind < argc; optind++)
    set_mbr(argv[optind]);
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
