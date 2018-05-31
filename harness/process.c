#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "harness.h"

void process_init()
{
  struct rlimit limit = {0};
  void *res;
  static void *const areas[] = {(void*)0x7000, (void*)0, (void*)0xf000};
  int i;

  /* Default to 1 second of CPU time.  That should be enough for
     anyone... */
  set_cpu_limit(1);

  /* Stop us dumping core, just because the V86 died. */
  if (setrlimit (RLIMIT_CORE, &limit))
  {
    perror("setrlimit(RLIMIT_CORE)");
    exit(1);
  }
  
  /* Claim the whole V86 area (except NULL). */
  res = mmap ((void*)0x1000, 0x10f000, 0,
	      MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
  if (res != (void*)0x1000)
  {
    if (res == (void*)-1)
      fprintf(stderr, "Failed to claim V86 area: %s\n", strerror(errno));
    else
      fprintf(stderr, "V86 area already occupied.\n");
    exit(1);
  }

  for (i=0; i<sizeof(areas)/sizeof(*areas); i++)
  {
    if (mmap (areas[i], 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
	      MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == (void*)-1)
    {
      fprintf(stderr, "Failed to map area at 0x%x: %s\n", (int)areas[i],
	      strerror(errno));
      exit(1);
    }
  }
}

void set_cpu_limit(unsigned long time)
{
  struct rlimit limit = {0};

  if (getrlimit (RLIMIT_CPU, &limit))
  {
    perror("getrlimit(RLIMIT_CPU)");
    exit(1);
  }
  
  limit.rlim_cur = time;

  if (setrlimit (RLIMIT_CPU, &limit))
  {
    perror("setrlimit(RLIMIT_CPU)");
    exit(1);
  }
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
