#include "harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static const char *mbr_file = NULL;
static unsigned char mbr_data[512] = {0};

void set_mbr(const char *filename)
{
  if (mbr_file)
  {
    fprintf(stderr, "MBR file already specified.\n");
    exit(1);
  }

  mbr_file = filename;
}

void mbr_init()
{
  unsigned char *code = (void*)0x7c00;
  FILE *f;
  size_t res;

  if (!mbr_file)
  {
    fprintf(stderr, "MBR file not specified.\n");
    exit(1);
  }

  f = fopen(mbr_file, "rb");
  if (!f)
  {
    fprintf(stderr, "Could not open %s for reading: %s\n", mbr_file,
	    strerror(errno));
    exit(1);
  }
  res = fread(mbr_data, 1, 512, f);
  if (res != 512)
  {
    if (ferror(f))
    {
      fprintf(stderr, "Error reading from %s: %s\n", mbr_file,
	      strerror(errno));
      exit(1);
    }
    else
    {
      fprintf(stderr, "EOF reading from %s.\n", mbr_file);
      exit(1);
    }
  }
  fclose(f);

  /* Copy it into the VM. */
  memcpy(code, mbr_data, sizeof(mbr_data));

  /* Set the program start. */
  set_pc (0, 0x7c00);
}

void compare_mbr(const unsigned char *data)
{
  unsigned int i;
  unsigned int last = 1024; /* Force a sync. */
  for (i=0; i<512; i++)
  {
    /* Write out all differences. */
    if (data[i] != mbr_data[i])
    {
      /* Write the address if we want the synchronise. */
      if ((i-last) >= 16)
      {
	outputf("At address 0x%x:\n", i);
	last = i;
      }
      outputf("Changed 0x%02x to 0x%02x.\n", mbr_data[i], data[i]);
    }
    else
      /* Force a sync next time since the sequence is broken. */
      last = 1024;
  }
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
