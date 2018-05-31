/* install-mbr for installing a master boot record */
#define COPYRIGHT "Copyright (C) 1999-2001 Neil Turton."
/* See file AUTHORS for a list of contributors.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

See file COPYING for details */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include "mbr.h"

typedef u_int8_t  ui8_t;
typedef u_int16_t ui16_t;

#define BUFFER_SIZE 512
#define CODE_SIZE (512-66)
#define PTN_TABLE_OFFSET CODE_SIZE
#define PTN_TABLE_SIZE 66
#define SIG_PTR_OFFSET (CODE_SIZE-2)

#define DEFAULT_TIMEOUT 18

const char *prog_name="install-mbr";

/* There are 3 routines (and their helpers) which need to know ALL the
 * members of this structure.  These are: modify_params fetch_params
 * and merge_changes.*/
struct change_params {
  /* Version numbers can't be changed... */
  ui8_t  compat_version;
  ui8_t  version;
  /* This now describes which fields are valid */
  ui8_t flags;
#define CPFLAG_DRIVE              1
#define CPFLAG_DEFAULT            2
#define CPFLAG_TIMEOUT_VALID      4
  /* The following bit is used to resolve conflicts between
   * "-t <timeout>" and "-i a".  It is set by "-t <timeout>" and
   * cleared by "-i a".  If it is set, "-i a" is ignored.  This allows
   * "-t <timeout>" followed by "-i a-a" to work. */
#define CPFLAG_TIMEOUT_OVERRIDES  8
#define CPFLAG_ENABLED           16
#define CPFLAG_INTERRUPT         32
#define CPFLAG_Y2KBUG            64 /* Selects default code. */
#define CPFLAG_Y2KDATE          128 /* Requires CPFLAG_Y2KBUG */

  /* Requires CPFLAG_DRIVE */
  ui8_t  drive;

  /* Requires CPFLAG_DEFAULT */
  ui8_t  deflt;

  /* Requires CPFLAG_TIMEOUT_VALID */
  ui16_t timeout;

  /* Must be initialised.  Can be ignored unless CPFLAG_ENABLED */
  ui8_t  enabled, enabled_mask;
#define CPE_1      ((ui8_t)1)
#define CPE_2      ((ui8_t)2)
#define CPE_3      ((ui8_t)4)
#define CPE_4      ((ui8_t)8)
#define CPE_F      ((ui8_t)16)
#define CPE_A      ((ui8_t)128)

  /* Must be initialised.  Can be ignored unless CPFLAG_INTERRUPT */
  ui8_t  interrupt, interrupt_mask;
#define CPI_SHIFT  1
#define CPI_KEY   2
#define CPI_ALWAYS 4
#define CPI_MASK   (CPI_SHIFT|CPI_KEY|CPI_ALWAYS)

  /* Requires CPFLAG_Y2KBUG */
  ui8_t  y2k_fix;

  /* Requires CPFLAG_Y2KDATE */
  ui8_t  y2k_month;
  ui16_t y2k_year;
};

struct file_desc {
  ui8_t *data;
  char *path;
  int flags;
#define FILE_DESC_FLAG_FREE_DATA 1 /* Should we free the data? */
#define FILE_DESC_FLAG_SPECIFIED 2
};

struct data {
  struct file_desc install, params, target, table;
  unsigned int offset;
  int flags;
#define DFLAG_KEEP     1
#define DFLAG_RESET    2
#define DFLAG_LIST     4
#define DFLAG_NO_ACT   8
#define DFLAG_VERBOSE 16
#define DFLAG_FORCE   32
  struct change_params changes;
};

/* Version specific routines *********************************************/

/* Locating parameters */

static struct mbr_params_v1 *
find_params_v1(const struct file_desc *dsc, int quiet)
{
  ui8_t *mbr=dsc->data;
  uint ptr;
  struct mbr_params_v1 *params;
  uint ver_compat;

#if 0
  fprintf(stderr, "Looking for params at %p\n", mbr);
#endif

  ptr=mbr[SIG_PTR_OFFSET]+256*mbr[SIG_PTR_OFFSET+1];
  if(ptr>SIG_PTR_OFFSET-(6+4+4) ||
     memcmp(mbr+ptr, MP_V1_SIG, sizeof(params->sig))!=0)
  {
    /* Due to a silly little mistake, version 1.0.0 got released with
       the signature pointer uninitialised.  This means that everyone
       has a different pointer, but the signature in this version is
       at 0x13b */
    ptr=0x13b;
    /* Parameters not found. */
    if(memcmp(mbr+ptr, MP_V1_SIG, sizeof(MP_V1_SIG)))
	return NULL;
  }
  params=(struct mbr_params_v1 *)(mbr+ptr);
  ver_compat=params->ver_compat;
  if(ver_compat > MP_V1_VERSION)
  {
    uint version=params->version;

    /* FIXME: We should be able to override this error message. */
    fprintf(stderr, "%s:%s: Cannot handle MBR version %d "
	    "(backwards compatible to %d)\n",
	    prog_name, dsc->path, version, ver_compat);
    exit(1);
  }
  return params;
}

static struct mbr_params_v2 *
find_params_v2(const struct file_desc *dsc, int quiet)
{
  /* The parameters end just before the partition table, at 512-66. */
  static const int offset = (512-66-sizeof(struct mbr_params_v2));
  static const char sig[] = MP_V2_SIG;
  struct mbr_params_v2 *params = (void*)(dsc->data + offset);

  /* Check the signature. */
  if (memcmp(params->sig, sig, sizeof(sig)))
    return NULL;

  /* Check the version. */
  if (params->version < MP_V2_VERSION_MIN ||
      params->ver_compat < MP_V2_VERSION_MIN ||
      params->ver_compat > MP_V2_VERSION)
  {
    /* FIXME: We should be able to override this error message. */
    fprintf(stderr, "%s:%s: Cannot handle MBR version %d "
	    "(backwards compatible to %d)\n",
	    prog_name, dsc->path, params->version, params->ver_compat);
    exit(1);
  }

  return params;
}

/*************************************************************************/

/* Modifying parameters.  First, we have some individual routines to
   set specific fields. */

/* This has no power to change anything.  It just makes sure that the
   request is compatible. */
static void
check_variant(ui8_t variant, const struct change_params *cpp)
{
  if (cpp->flags & CPFLAG_Y2KBUG)
  {
    /* Make sure the settings match. */
    if (cpp->y2k_fix != ((variant & MP_VARIANT_Y2K) ? 1 : 0))
    {
      fprintf(stderr, "%s: Selected code does not have requested "
	      "Y2K bug fix setting.\n", prog_name);
      exit(1);
    }
  }
}

static void
modify_drive(ui8_t *drivep, const struct change_params *cpp)
{
  if(cpp->flags & CPFLAG_DRIVE)
    *drivep=cpp->drive;
}

static void
modify_default(ui8_t *defaultp, const struct change_params *cpp)
{
  if(cpp->flags & CPFLAG_DEFAULT)
  {
    *defaultp&=~MP_DEFLT_BITS;
    switch(cpp->deflt)
    {
    case '1':           *defaultp|=0; break;
    case '2':           *defaultp|=1; break;
    case '3':           *defaultp|=2; break;
    case '4':           *defaultp|=3; break;
    case 'F': case 'f': *defaultp|=4; break;
    case 'D': case 'd': *defaultp|=7; break;

    default:
      fprintf(stderr, "%s: Invalid default partition: %c\n",
	      prog_name, cpp->deflt);
      exit(1);
    };
  }

  /* The interrupt flags are stored here, too. */
  if(cpp->interrupt & CPI_SHIFT)
    *defaultp|=MP_DEFLT_ISHIFT;
  else if((cpp->interrupt_mask & CPI_SHIFT)==0)
    *defaultp&=~MP_DEFLT_ISHIFT;

  if(cpp->interrupt & CPI_KEY)
    *defaultp|=MP_DEFLT_IKEY;
  else if((cpp->interrupt_mask & CPI_KEY)==0)
    *defaultp&=~MP_DEFLT_IKEY;
}

static ui16_t
modify_timeout_sub(ui16_t timeout, const struct change_params *cpp)
{
  /* The interaction of the timeout with the interrupt always bit is a
     little subtle.
     CPIA_MASK CPIA TIMEOUT TIMEOUT_VAL comment
     X         1    0       X           Set always
     X         X    1       (1)         Set timeout
     X         0    X       1           Set timeout
     0         0    0       0           Set default timeout
     1         0    0       0           Nothing set - do nothing
  */

  if((cpp->interrupt & CPI_ALWAYS) != 0 /* If setting CPI_ALWAYS */
     && (cpp->flags & CPFLAG_TIMEOUT_OVERRIDES) == 0) /* and not overridden */
    timeout=0xffff; /* Set to always interrupt */
  else if(cpp->flags & CPFLAG_TIMEOUT_VALID)
    timeout=cpp->timeout;
  /* Timeout is not valid and not setting CPI_ALWAYS, so the only
     action we can be taking is clearing CPI_ALWAYS */
  else if((cpp->interrupt_mask & CPI_ALWAYS)==0)
    timeout=DEFAULT_TIMEOUT;

  return timeout;
}

static void
modify_timeout_8(ui8_t *timeout, const struct change_params *cpp)
{
  ui16_t new_timeout = modify_timeout_sub(*timeout, cpp);
  /* Make sure the value is valid for the old MBR.  Valid values are
     0..0xfe and 0xffff. */
  if (new_timeout > 0xfe && new_timeout != 0xffff)
  {
    fprintf(stderr, "%s: Timeout (%d) too large for legacy MBR.  "
	    "Please upgrade.\n", prog_name, new_timeout);
    exit(1);
  }

  *timeout = new_timeout;
}

static void
modify_timeout_16(ui8_t timeout[2], const struct change_params *cpp)
{
  ui16_t delay = timeout[0] + 256*(ui16_t)timeout[1];
  delay = modify_timeout_sub(delay, cpp);
  timeout[0] = (delay>>0) & 0xff;
  timeout[1] = (delay>>8) & 0xff;
}

static void
modify_flags(ui8_t *flagsp, const struct change_params *cpp)
{
  assert(MP_FLAG_EN1 == CPE_1);
  assert(MP_FLAG_EN2 == CPE_2);
  assert(MP_FLAG_EN3 == CPE_3);
  assert(MP_FLAG_EN4 == CPE_4);
  assert(MP_FLAG_ENF == CPE_F);
  assert(MP_FLAG_ENA == CPE_A);
  *flagsp=(*flagsp & cpp->enabled_mask)|cpp->enabled;
}

static void
modify_date(ui8_t *date_ptr, const struct change_params *cpp)
{
  ui8_t month = cpp->y2k_month;
  ui16_t year = cpp->y2k_year;
  ui8_t year_hi, year_lo;
  if (! (cpp->flags & CPFLAG_Y2KDATE))
    return;

  assert (cpp->flags & CPFLAG_Y2KBUG);
  assert (cpp->y2k_fix);

  /* These are stored in BCD (yawn). */
  date_ptr[2]=(month/10<<4)|(month%10);
  year_hi = year/100;
  year_lo = year%100;
  date_ptr[0]=(year_lo/10<<4)|(year_lo%10);
  date_ptr[1]=(year_hi/10<<4)|(year_hi%10);
}

/* This is simply a case of applying the above routines. */
static int
modify_params_v1(struct file_desc *dsc, const struct change_params *cpp)
{
  struct mbr_params_v1 *params=find_params_v1(dsc, 0);
  assert(MP_V1_VERSION==2);

  if (params == NULL)
    return 0;

  check_variant(params->variant, cpp);

  /* This section of structure changes format. */
  if (params->version < MP_V1_FMTB_VER)
  {
    modify_drive(&(params->u.fmt_a.drive), cpp);
    modify_timeout_8(&(params->u.fmt_a.delay), cpp);
  }
  else
  {
    modify_drive(&(params->u.fmt_b.drive), cpp);
    modify_timeout_16(params->u.fmt_b.delay, cpp);
  }
  modify_default(&(params->deflt), cpp);
  modify_flags(&(params->flags), cpp);
  modify_date(dsc->data+SIG_PTR_OFFSET-3, cpp);

  return 1;
}

static int
modify_params_v2(struct file_desc *dsc, const struct change_params *cpp)
{
  struct mbr_params_v2 *params=find_params_v2(dsc, 0);
  assert(MP_V2_VERSION==2);

  if (params == NULL)
    return 0;

  check_variant(params->variant, cpp);
  modify_drive(&(params->drive), cpp);
  modify_default(&(params->deflt), cpp);
  modify_timeout_16(params->delay, cpp);
  modify_flags(&(params->flags), cpp);
  modify_date(params->date, cpp);

  return 1;
}

static void
modify_params(struct file_desc *dsc, const struct change_params *cpp)
{
  if (modify_params_v1(dsc, cpp) ||
      modify_params_v2(dsc, cpp))
    return;

  /* We can't continue. */
  fprintf(stderr, "%s:%s: Failed to find MBR signature.\n",
	  prog_name, dsc->path);
  exit(1);
}

/* Fetching parameters.  First, we have some helper routines. */

/* This must be called before the others. */
static void
handle_version(struct change_params *cpp, ui8_t compat, ui8_t ver)
{
  cpp->compat_version = compat;
  cpp->version = ver;
  cpp->flags = CPFLAG_INTERRUPT;
  cpp->interrupt=0;
  cpp->interrupt_mask=~0;
  cpp->enabled=0;
  cpp->enabled_mask=~0;
}

static void
handle_variant(struct change_params *cpp, ui8_t variant)
{
  if (cpp->version >= MP_VERSION_Y2K)
  {
    cpp->flags |= CPFLAG_Y2KBUG;
    cpp->y2k_fix = (variant & MP_VARIANT_Y2K) ? 1 : 0;
  }
}

static void
handle_drive(struct change_params *cpp, ui8_t drive)
{
  cpp->flags|=CPFLAG_DRIVE;
  cpp->drive=drive;
}

static void
handle_default(struct change_params *cpp, ui8_t deflt)
{
  cpp->flags|=CPFLAG_DEFAULT;
  switch(deflt & MP_DEFLT_BITS)
  {
  case 0: cpp->deflt='1'; break;
  case 1: cpp->deflt='2'; break;
  case 2: cpp->deflt='3'; break;
  case 3: cpp->deflt='4'; break;
  case 4: cpp->deflt='F'; break;
  case 7: cpp->deflt='D'; break;
  default:
      fprintf(stderr, "%s: Invalid default partition found: %d\n",
	      prog_name, deflt);
      exit(1);
  };

  cpp->interrupt_mask&=~(CPI_SHIFT|CPI_KEY);
  if(deflt & MP_DEFLT_ISHIFT)
    cpp->interrupt|=CPI_SHIFT;
  if(deflt & MP_DEFLT_IKEY)
    cpp->interrupt|=CPI_KEY;
}

static void
handle_timeout_8(struct change_params *cpp, ui8_t delay)
{
  cpp->interrupt_mask&=~CPI_ALWAYS;
  if(delay == 0xff)
  {
    cpp->interrupt|=CPI_ALWAYS;
  }
  else
  {
    cpp->timeout=delay;
    cpp->flags|=CPFLAG_TIMEOUT_VALID|CPFLAG_TIMEOUT_OVERRIDES;
  }
}

static void
handle_timeout_16(struct change_params *cpp, const ui8_t delay_p[2])
{
  u_int16_t delay = delay_p[0] + 256*(ui16_t)delay_p[1];
  cpp->interrupt_mask&=~CPI_ALWAYS;
  if(delay == 0xffff)
  {
    cpp->interrupt|=CPI_ALWAYS;
  }
  else
  {
    cpp->timeout=delay;
    cpp->flags|=CPFLAG_TIMEOUT_VALID|CPFLAG_TIMEOUT_OVERRIDES;
  }
}

static void
handle_enable(struct change_params *cpp, ui8_t flags)
{
  assert(MP_FLAG_EN1 == CPE_1);
  assert(MP_FLAG_EN2 == CPE_2);
  assert(MP_FLAG_EN3 == CPE_3);
  assert(MP_FLAG_EN4 == CPE_4);
  assert(MP_FLAG_ENF == CPE_F);
  assert(MP_FLAG_ENA == CPE_A);
  cpp->enabled=flags;
  cpp->enabled_mask=(ui8_t)~(ui8_t)(CPE_1|CPE_2|CPE_3|CPE_4|CPE_F|CPE_A);
  cpp->flags|=CPFLAG_ENABLED;
}

/* Fetch the parameters from src to cp.  If quiet is set, return false
   on failure, instead of calling exit(1). */
static int
fetch_params_v1(struct change_params *cpp, const struct file_desc *src)
{
  const struct mbr_params_v1 *params=find_params_v1(src, 1);
  assert(MP_V1_VERSION==2);
  if(params==NULL)
    return 0;
  
  handle_version(cpp, params->ver_compat, params->version);
  handle_variant(cpp, params->variant);
  /* This section of structure changes format. */
  if (params->version < MP_V1_FMTB_VER)
  {
    handle_drive(cpp, params->u.fmt_a.drive);
    handle_timeout_8(cpp, params->u.fmt_a.delay);
  }
  else
  {
    handle_drive(cpp, params->u.fmt_b.drive);
    handle_timeout_16(cpp, params->u.fmt_b.delay);
  }
  handle_default(cpp, params->deflt);
  handle_enable(cpp, params->flags);
  return 1;
}

static int
fetch_params_v2(struct change_params *cpp, const struct file_desc *src)
{
  const struct mbr_params_v2 *params=find_params_v2(src, 1);
  assert(MP_V2_VERSION==2);
  if(params==NULL)
    return 0;
  
  handle_version(cpp, params->ver_compat, params->version);
  handle_variant(cpp, params->variant);
  handle_drive(cpp, params->drive);
  handle_default(cpp, params->deflt);
  handle_timeout_16(cpp, params->delay);
  handle_enable(cpp, params->flags);
  return 1;
}

static int
fetch_params(struct change_params *cpp, const struct file_desc *src, int quiet)
{
  if (fetch_params_v1(cpp, src) ||
      fetch_params_v2(cpp, src))
    return 1;

  /* We didn't find anything. */
  if (quiet)
    return 0;

  /* We can't continue. */
  fprintf(stderr, "%s:%s: Failed to find MBR signature.\n",
	  prog_name, src->path);
  exit(1);
}


/* Data handling *********************************************************/

/* This copies values from src to dest.  Any value which is specified
   in either original structure will be specified in the result.
   Values in src override values in dest. */
static void
merge_changes(struct change_params *dest, const struct change_params *src)
{
  int src_flags = src->flags;

  /* We ignore the version numbers.  The dest should be right. */

  /* Merge the flags.  This is usually the right thing to do, but see
     below. */
  dest->flags |= src_flags;

  if (src_flags & CPFLAG_DRIVE)
    dest->drive = src->drive;

  if (src_flags & CPFLAG_DEFAULT)
    dest->deflt = src->deflt;
  
  if (src_flags & CPFLAG_TIMEOUT_VALID)
    dest->timeout = src->timeout;

  /* If the source timeout doesn't override and interrupt has been
     specified, make it so in the result. */
  if ((src->interrupt & CPI_ALWAYS) != 0
      && (src_flags & CPFLAG_TIMEOUT_OVERRIDES) == 0)
    dest->flags &= ~CPFLAG_TIMEOUT_OVERRIDES;

  dest->enabled &= src->enabled_mask;
  dest->enabled_mask &= src->enabled_mask;
  dest->enabled |= src->enabled;

  dest->interrupt &= src->interrupt_mask;
  dest->interrupt_mask &= src->interrupt_mask;
  dest->interrupt |= src->interrupt;

  if (src_flags & CPFLAG_Y2KBUG)
    dest->y2k_fix = src->y2k_fix;

  if (src_flags & CPFLAG_Y2KDATE)
  {
    dest->y2k_month = src->y2k_month;
    dest->y2k_year  = src->y2k_year;
  }
}

static void show_params(const struct change_params *cp)
{
  printf("Version: %d\n", cp->version);
  printf("Compatible: %d\n", cp->compat_version);
  if(cp->flags & CPFLAG_Y2KBUG)
    printf("Y2K-Fix: %s\n", cp->y2k_fix ? "Enabled" : "Disabled");
  if(cp->flags & CPFLAG_DRIVE)
    printf("Drive: 0x%x\n", cp->drive);
  if(cp->flags & CPFLAG_DEFAULT)
    printf("Default: %c\n", cp->deflt);
  if(cp->flags & CPFLAG_TIMEOUT_OVERRIDES)
    printf("Timeout: %d/18 seconds\n", cp->timeout);
  if(cp->flags & CPFLAG_ENABLED)
  {
    printf("Enabled:");
    if(cp->enabled & CPE_1)
      printf(" 1");
    if(cp->enabled & CPE_2)
      printf(" 2");
    if(cp->enabled & CPE_3)
      printf(" 3");
    if(cp->enabled & CPE_4)
      printf(" 4");
    if(cp->enabled & CPE_F)
      printf(" F");
    if(cp->enabled & CPE_A)
      printf(" A");
      printf("\n");
  }
  if(cp->flags & CPFLAG_INTERRUPT)
  {
    printf("Interrupt:");
    if(cp->interrupt & CPI_SHIFT)
      printf(" Shift");
    if(cp->interrupt & CPI_KEY)
      printf(" Key");
    if(cp->interrupt & CPI_ALWAYS)
      printf(" Always");
    printf("\n");
  }
}

static void
show_mbr(struct file_desc *dsc)
{
  struct change_params cp;
  fetch_params(&cp, dsc, 0);
  show_params(&cp);
}

static void
copy_code(struct file_desc *dest, const struct file_desc *src)
{
  memcpy(dest->data, src->data, CODE_SIZE);
  /* Any errors should be reported against the source. */
  dest->path = src->path;
}

/* Copy the partition table */
static void
copy_table(struct file_desc *dest, const struct file_desc *src)
{
  memcpy(dest->data+PTN_TABLE_OFFSET, src->data+PTN_TABLE_OFFSET,
	 PTN_TABLE_SIZE);
}

/* IO routines ***********************************************************/

typedef ssize_t io_func(int, void *, size_t);

static size_t
do_io(io_func *fn, const struct file_desc *dsc, int fd, size_t min_size)
{
  int res;
  size_t done=0;
  while(done<BUFFER_SIZE)
  {
    res=fn(fd, dsc->data, BUFFER_SIZE-done);
    if(res<0 && errno!=EINTR)
    {
      fprintf(stderr, "%s: Error reading %s: %s\n", prog_name,
	      dsc->path, strerror(errno));
      exit(1);
    }
    if(res==0)
    {
      if(done>=min_size)
	return done;
      fprintf(stderr, "%s: Unexpected EOF while reading %s\n",
	      prog_name, dsc->path);
      exit(1);
    }
    done+=res;
  }
  return done;
}

static size_t
read_from_fd(const struct file_desc *dsc, int fd, size_t min_size)
{
  return do_io(read, dsc, fd, min_size);
}

/* Bah.  The prototype for write is different from that for read by
   a const.  Surely it is compatible, but ANSI C say no. */
static ssize_t
my_write(int fd, void *buf, size_t size)
{
  return write(fd, buf, size);
}

static size_t
write_to_fd(const struct file_desc *dsc, int fd, size_t min_size)
{
  return do_io(my_write, dsc, fd, min_size);
}

static void
read_file(struct file_desc *dsc)
{
  int fd=open(dsc->path, O_RDONLY|O_NOCTTY);
  if(fd==-1)
  {
    fprintf(stderr, "%s: Failed to open %s: %s\n",
	    prog_name, dsc->path, strerror(errno));
    exit(1);
  }
  if(dsc->flags & FILE_DESC_FLAG_FREE_DATA)
    free(dsc->data);
  dsc->data=malloc(512);
  if(dsc->data==NULL)
  {
    fprintf(stderr, "%s: Malloc failed\n", prog_name);
    exit(1);
  }
  dsc->flags|=FILE_DESC_FLAG_FREE_DATA;
  read_from_fd(dsc, fd, BUFFER_SIZE);
  if(close(fd)==-1)
  {
    fprintf(stderr, "%s: Error during close of %s: %s\n",
	    prog_name, dsc->path, strerror(errno));
    exit(1);
  }
}

/* The install code ******************************************************/

static void
install_mbr(struct data *dp)
{
  struct change_params new_params;
  int mbr_fd = -1;
  ui8_t mbr_in[BUFFER_SIZE];
  ui8_t mbr_out[BUFFER_SIZE];
  struct file_desc target_orig={mbr_in, dp->target.path, 0};

  memset(mbr_in, 0, BUFFER_SIZE);

  dp->target.data=mbr_out;

  /* We build up a change_params structure for all the parameters, so
   * we can pick out things like the Y2K flag...
   *
   * Priorities:
   *  1. Command line options.
   *  2. Params file (if specified).
   *  3. Original mbr unless -r specified.
   *  4. Installed mbr (even if -k specified).
   *
   * We process these from 3 down to 1, and then apply the result to
   * 4.
   */

  /* Start with an empty record. */
  memset(mbr_out, 0, sizeof(mbr_out));

  /* Open the target file/device. */
  mbr_fd=open(dp->target.path,
	      ((dp->flags & DFLAG_NO_ACT)
	       ? (O_RDONLY|O_NOCTTY)
	       : (O_RDWR|O_NOCTTY))
	      | ((dp->flags & DFLAG_FORCE) ? O_CREAT : 0),
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

  if(mbr_fd==-1)
  {
    fprintf(stderr, "%s: Failed to open %s: %s\n",
	    prog_name, dp->target.path, strerror(errno));
    exit(1);
  }

  /* Read the old MBR in.  This contains at least the partition
     table. */
  if(lseek(mbr_fd, dp->offset, SEEK_SET)==-1)
  {
    fprintf(stderr, "%s:%s: Failed to seek: %s\n",
	    prog_name, dp->target.path, strerror(errno));
    exit(1);
  }
  read_from_fd(&target_orig, mbr_fd,
	       dp->flags & DFLAG_FORCE ? 0 : BUFFER_SIZE);
  /* Check the signature */
  if ((dp->flags & DFLAG_FORCE) == 0 &&
      (mbr_in[BUFFER_SIZE-2] != 0x55 ||
       mbr_in[BUFFER_SIZE-1] != 0xAA))
  {
    fprintf(stderr, "%s:%s: No boot signature found.  "
	    "Use --force to override.\n", prog_name,
	    dp->target.path);
    exit(1);
  }

  /* Try to extract the parameters unless we're resetting them.  If we
     are resetting, or the extraction fails, reset them anyway. */
  if ((dp->flags & DFLAG_RESET) != 0 ||
      !fetch_params(&new_params, &target_orig, 1))
  {
    /* These aren't really needed. */
    new_params.compat_version = 0;
    new_params.version = 0;
    /* Reset everything. */
    new_params.flags = 0;
    new_params.interrupt = 0;
    new_params.interrupt_mask = ~0;
    new_params.enabled = 0;
    new_params.enabled_mask = ~0;
  }

  /* Read the parameter file if specified. */
  if(dp->params.flags & FILE_DESC_FLAG_SPECIFIED)
  {
    struct change_params extra_params;
    read_file(&(dp->params));
    fetch_params(&extra_params, &(dp->params), 0);
    merge_changes(&new_params, &extra_params);
  }
  else
  {
    dp->params.path=0;
    dp->params.data=0;
    dp->params.flags=0;
  }

  /* If a file to install is specified, ignore the Y2K setting from
     the original target and the params file. */
  if (dp->install.flags & FILE_DESC_FLAG_SPECIFIED)
    new_params.flags &= ~(CPFLAG_Y2KBUG|CPFLAG_Y2KDATE);

  /* Merge in the command line changes. */
  merge_changes(&new_params, &(dp->changes));

  /* Read the code file if specified.  Otherwise, use the
     built-in version. */
  if(dp->install.flags & FILE_DESC_FLAG_SPECIFIED)
    read_file(&(dp->install));
  else
  {
    /* Check if the Y2K flag is set.  */
    if (new_params.flags & CPFLAG_Y2KBUG ?
	new_params.y2k_fix : 0)
    {
      dp->install.path="<internal-y2k>";
      dp->install.data=Y2K_START;
    }
    else
    {
      dp->install.path="<internal>";
      dp->install.data=MBR_START;
    }
    dp->install.flags=0;
  }

  if(dp->table.flags & FILE_DESC_FLAG_SPECIFIED)
    read_file(&(dp->table));
  else
  {
    dp->table.data = target_orig.data;
    dp->table.path = target_orig.path;
    dp->table.flags = 0;
  }

  /* Copy the code unless the user asks not to. */
  if((dp->flags & DFLAG_KEEP)==0)
  {
    if(dp->flags & DFLAG_VERBOSE)
      fprintf(stderr, "Copying code from %s\n", dp->install.path);
    copy_code(&(dp->target), &(dp->install));
  } else {
    if(dp->flags & DFLAG_VERBOSE)
      fprintf(stderr, "Copying code from %s\n", target_orig.path);
    copy_code(&(dp->target), &target_orig);
  }

  /* Modify the parameters according to the change_params structure. */
  if(new_params.flags)
  {
    if(dp->flags & DFLAG_VERBOSE)
      fprintf(stderr,"Modifying parameters.\n");

    modify_params(&(dp->target), &new_params);
  }

  /* Copy the partition table. */
  copy_table(&(dp->target), &(dp->table));

  if(dp->flags & DFLAG_LIST)
    show_mbr(&(dp->target));

  /* Stamp the boot signature on to the result (just in case). */
  mbr_out[BUFFER_SIZE-2]=0x55;
  mbr_out[BUFFER_SIZE-1]=0xAA;

  if((dp->flags & DFLAG_NO_ACT) == 0)
  {
    /* Write the result out. */
    if(lseek(mbr_fd, dp->offset, SEEK_SET)==-1)
    {
      fprintf(stderr, "%s:%s: Failed to seek: %s\n",
	      prog_name, dp->target.path, strerror(errno));
      exit(1);
    }
    write_to_fd(&(dp->target), mbr_fd, BUFFER_SIZE);
  };

  /* Close the target */
  if(close(mbr_fd))
  {
    fprintf(stderr, "%s: Error while closing %s: %s\n",
	    prog_name, dp->target.path, strerror(errno));
    exit(1);
  };
}

/* Utility function ******************************************************/

static unsigned int
get_uint(const char *str, const char *name)
{
  char *end_ptr;
  unsigned int result;
  if(str!=NULL && str[0]!=0)
  {
    result=(int)strtoul(str, &end_ptr, 0);
    if(*end_ptr==0)
      return result;
  }
  fprintf(stderr, "%s: %s must be a number: %s\n",
	  prog_name, name, str);
  exit(1);
}

/* Option processing *****************************************************/

static void
show_usage(FILE *f)
{
  fprintf(f, "Usage: %s [options] <target>\n", prog_name);
}

static void
suggest_help(void)
{
  fprintf(stderr, "Try '%s --help' for more information.\n",
	  prog_name);
}

static void
show_help(void)
{
  show_usage(stdout);
  printf("Options:\n"
	 "  -f, --force                       "
	 "Override some sanity checks.\n"
	 "  -I <path>, --install <path>       "
	 "Install code from the specified file.\n"
	 "  -k, --keep                        "
	 "Keep the current code in the MBR.\n"
	 "  -l, --list                        "
	 "Just list the parameters.\n"
	 "  -n, --no-act                      "
	 "Don't install anything.\n"
	 "  -o <offset>, --offset <offset>    "
	 "Install the MBR at byte offset <offset>.\n"
	 "  -y[u|l|-], --y2kbug[=utc|=local|=off]\n"
	 "                                    "
	 "Fix a Y2K bug of some BIOS.\n"
	 "  -P <path>, --parameters <path>    "
	 "Get parameters from <path>.\n"
	 "  -r, --reset                       "
	 "Reset the parameters to the default state.\n"
	 "  -T <path>, --table <path>         "
	 "Get partition table from <path>.\n"
	 "  -v, --verbose                     "
	 "Operate verbosely.\n"
	 "  -V, --version                     "
	 "Show version.\n"
	 "  -h, --help                        "
	 "Display this message.\n"
	 "Parameters:\n"
	 "  -d <drive>, --drive <drive>       "
	 "Set BIOS drive number.\n"
	 "  -e <option>, --enable <option>    "
	 "Select enabled boot option.\n"
	 "  -i <mode>, --interrupt <keys>     "
	 "Set interrupt mode. (a/c/s/cs/n)\n"
	 "  -p <partn>, --partition <partn>   "
	 "Set boot partition (0=whole disk).\n"
	 "  -t <timeout>, --timeout <timeout> "
	 "Set the timeout in 1/18 second.\n"
	 "Interrupt modes:\n"
	 "  's'=Interrupt if shift or ctrl is pressed.\n"
	 "  'k'=Interrupt if other key pressed.\n"
	 "  'a'=Interrupt always.\n"
	 "  'n'=Interrupt never.\n"
	 "Boot options:\n"
	 "  '1','2','3' or '4' - Partition 1,2,3 or 4.\n"
	 "  'F' - 1st floppy drive.\n"
	 "  'A' - Advanced mode.\n"
	 "Report bugs to neilt@chiark.greenend.org.uk\n"
	 );
}

static void
parse_enabled(struct change_params *params, const char *arg)
{
  char mode='+';
  if((params->flags & CPFLAG_ENABLED)==0)
  {
    /* The first time round, default to override */
    mode='=';
    params->flags|=CPFLAG_ENABLED;
  }

  while(*arg)
  {
    char c=*arg++;
    ui8_t bit=0;
    switch(c)
    {
    case '=':
      params->enabled_mask=0;
      params->enabled=0;
      /* Fall through */
    case '+': case '-':
      mode=c;
      /* Skip to the next iteration round the loop */
      continue;

    case '1': bit=CPE_1; break;
    case '2': bit=CPE_2; break;
    case '3': bit=CPE_3; break;
    case '4': bit=CPE_4; break;
    case 'F': case 'f': bit=CPE_F; break;
    case 'A': case 'a': bit=CPE_A; break;

    default:
      fprintf(stderr, "%s: Invalid boot option: %c\n", prog_name, c);
      exit(1);
    }
    if(mode=='=')
      params->enabled_mask=0;
    else
      params->enabled_mask&=~bit;

    if(mode=='-')
      params->enabled&=~bit;
    else
      params->enabled|=bit;
  }
}

static void
parse_interrupt(struct change_params *params, const char *arg)
{
  char mode='+';
  if((params->flags & CPFLAG_INTERRUPT)==0)
  {
    /* The first time round, default to override */
    mode='=';
    params->flags|=CPFLAG_INTERRUPT;
  }
  while(*arg)
  {
    char c=*arg++;
    ui8_t bit=0;
    switch(c)
    {
    case '=':
      params->interrupt_mask=0;
      params->interrupt=0;
      /* Fall through */
    case '+': case '-':
      mode=c;
      /* Skip to the next iteration round the loop */
      continue;

    case 's': case 'S': bit=CPI_SHIFT; break;
    case 'k': case 'K': bit=CPI_KEY; break;
    case 'a': case 'A': bit=CPI_ALWAYS;
      if(mode!='-') /* Don't enforce the timeout */
	params->flags&=~CPFLAG_TIMEOUT_OVERRIDES;
      break;
    case 'n': case 'N':
      params->interrupt_mask&=~CPI_MASK;
      if(mode=='-') {
	params->interrupt|=CPI_MASK;
      } else {
	params->interrupt&=~CPI_MASK;
      };
      continue;

    default:
      fprintf(stderr, "%s: Invalid interrupt option: %c\n", prog_name, c);
      exit(1);
    }
    if(mode=='=')
      params->interrupt_mask=0;
    else
      params->interrupt_mask&=~bit;

    if(mode=='-')
      params->interrupt&=~bit;
    else
      params->interrupt|=bit;
  }
}

static void
parse_options(struct data *my_data, int argc, char *argv[])
{
  int c;
  char to_do=0;
  time_t atime;
  struct tm btime;
  static struct option long_options[] =
  {
    /* {name, has_arg, flag, val} */
    {"force",      0, 0, 'f'},
    {"install",    1, 0, 'I'},
    {"keep",       0, 0, 'k'},
    {"list",       0, 0, 'l'},
    {"no-act",     0, 0, 'n'},
    {"offset",     1, 0, 'o'},
    {"y2kbug",     2, 0, 'y'},
    {"parameters", 1, 0, 'P'},
    {"reset",      0, 0, 'r'},
    {"table",      1, 0, 'T'},
    {"verbose",    0, 0, 'v'},
    {"version",    0, 0, 'V'},
    {"help",       0, 0, 'h'},

    {"drive",      1, 0, 'd'},
    {"enabled",    1, 0, 'e'},
    {"interrupt",  1, 0, 'i'},
    {"partition",  1, 0, 'p'},
    {"timeout",    1, 0, 't'},
    {0, 0, 0, 0},
  };

  prog_name=strrchr(argv[0], '/');
  if(prog_name==NULL)
    prog_name=argv[0];
  else
    prog_name++;

  while(1)
  {
    int long_index=0;
    c=getopt_long(argc, argv, "fI:klno:y::P:rT:vVhd:e:i:p:t:",
		  long_options, &long_index);
    if(c==EOF)
      break;

    switch(c)
    {
    case 'f':
      my_data->flags |= DFLAG_FORCE;
      break;

    case 'I': /* code to install */
      to_do=1;
      my_data->install.path=optarg;
      my_data->install.flags=FILE_DESC_FLAG_SPECIFIED;
      break;

    case 'k': /* Keep the current code */
      to_do=1;
      my_data->flags |= DFLAG_KEEP;
      break;

    case 'l': /* List mode */
      my_data->flags |= DFLAG_LIST;
      break;

    case 'n': /* Don't install */
      my_data->flags |= DFLAG_NO_ACT;
      break;

    case 'o': /* Offset */
      my_data->offset=get_uint(optarg, "Offset");
      break;

    case 'y': /* y2kbug */
      to_do=1;
      if (!optarg)
      {
	/* Try to determine the which timezone the RTC is set to. */
	FILE *debdef=fopen("/etc/default/rcS", "r");
	/* Default to UTC */
	optarg="u";
	if(debdef == NULL)
	{
	  if (errno!=ENOENT)
	  {
	    perror("Could not open /etc/default/rcS");
	    exit(1);
	  }
	}
	else
	{
	  char line[8];
	  int c;
	  while (fgets(line, 8, debdef))
	  {
	    if (!memcmp("UTC=no\n", line, 7)) {
	      optarg="l";
	      break;
	    }
	    if (strchr(line, '\n') == NULL)
	    {
	      do { c = getc(debdef); } while (c != EOF && c != '\n');
	      if (c == EOF)
		break;
	    }
	  }
	  if (ferror (debdef) || fclose(debdef))
	  {
	    perror("Error reading /etc/default/rcS");
	    exit(1);
	  }
	}
      }
      if (!strcmp(optarg, "-") ||
	  !strcmp(optarg, "off"))
      {
	/* Request to turn Y2K bug off. */
	my_data->changes.flags |= CPFLAG_Y2KBUG;
	my_data->changes.y2k_fix = 0;
      }
      else
      {
	if (time(&atime) == (time_t)-1)
	{
	  fprintf(stderr, "%s: Error reading current time: %s\n",
		  prog_name, strerror(errno));
	  exit(1);
	}
	/* Valid arguments are "l", "u", "local" and "utc". */
	if (optarg[0] == 'l'?optarg[1]!=0&&strcmp(optarg+1,"ocal"):
	    optarg[0] == 'u'?optarg[1]!=0&&strcmp(optarg+1,"tc"):
	    1)
	{
	  fprintf(stderr, "%s: Invalid timezone after Y2K option: %s\n",
		  prog_name, optarg);
	  exit(1);
	}
        if (optarg[0]=='l')
          btime = *localtime(&atime);
        else
          btime = *gmtime(&atime);
	/* We've got the date.  Store the changes. */
	my_data->changes.flags |= CPFLAG_Y2KDATE | CPFLAG_Y2KBUG;
	my_data->changes.y2k_fix = 1;
	my_data->changes.y2k_month = btime.tm_mon+1;
        my_data->changes.y2k_year = btime.tm_year+1900;
      }
      break;

    case 'P': /* Parameters from file */
      to_do=1;
      my_data->params.path=optarg;
      my_data->params.flags=FILE_DESC_FLAG_SPECIFIED;
      break;

    case 'r': /* Reset parameters */
      to_do=1;
      my_data->flags |= DFLAG_RESET;
      break;

    case 'T': /* Partition table from file */
      to_do=1;
      my_data->table.path=optarg;
      my_data->table.flags=FILE_DESC_FLAG_SPECIFIED;
      break;

    case 'v': /* verbose */
      my_data->flags |= DFLAG_VERBOSE;
      break;
      
    case 'V': /* version */
      printf("install-mbr %s\n", VERSION);
      printf(COPYRIGHT "\n");
      exit(0);
      break;

    case 'h': /* help */
      show_help();
      exit(0);

    case 'd': /* drive */
      to_do=1;
      {
	unsigned int drive=get_uint(optarg, "Drive");
	if(drive>0xfe)
	{
	  fprintf(stderr, "%s: Invalid drive number: 0x%x\n",
		  prog_name, drive);
	  exit(1);
	};
	my_data->changes.flags|=CPFLAG_DRIVE;
	my_data->changes.drive=drive;
      }
      break;

    case 'e':
      to_do=1;
      parse_enabled(&(my_data->changes), optarg);
      break;

    case 'i':
      to_do=1;
      parse_interrupt(&(my_data->changes), optarg);
      break;

    case 'p':
      to_do=1;
      if(optarg[0]==0 || optarg[1]!=0)
      {
	fprintf(stderr, "%s: Invalid partition number: %s\n",
		prog_name, optarg);
	exit(1);
      }
      my_data->changes.flags|=CPFLAG_DEFAULT;
      my_data->changes.deflt=*optarg;
      break;

    case 't':
      to_do=1;
      {
	unsigned int timeout=get_uint(optarg, "Timeout");
	if(timeout>0xfffe)
	{
	  fprintf(stderr, "%s: Invalid timeout: %d\n",
		  prog_name, timeout);
	  exit(1);
	}
	my_data->changes.flags|=CPFLAG_TIMEOUT_VALID|CPFLAG_TIMEOUT_OVERRIDES;
	my_data->changes.timeout=timeout;
      }
      break;

    default:
      fprintf(stderr, "%s: Bad return from getopt_long: %d(%c)\n",
	      prog_name, errno, c);
      /* Fall-through */
    case '?':
      suggest_help();
      exit(1);
    }
  }
  if(argc-optind!=1)
  {
    show_usage(stderr);
    suggest_help();
    exit(1);
  }
  my_data->target.path=argv[optind];
  my_data->target.flags=FILE_DESC_FLAG_SPECIFIED;

  /* Don't do anything if all we're asked to do is list. */
  if((my_data->flags & DFLAG_LIST) && !to_do)
    my_data->flags |= DFLAG_NO_ACT | DFLAG_KEEP;
}

int
main(int argc, char *argv[])
{
  /* FIXME: Use named fields */
  struct data my_data={
    /* install */
    {NULL, NULL, 0},
    /* params */
    {NULL, NULL, 0},
    /* target */
    {NULL, NULL, 0},
    /* table */
    {NULL, NULL, 0},
    /* offset */
    0,
    /* flags */
    0,
    /* change_params structure */
    {0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0xff, 0 }};

  assert(MBR_SIZE==BUFFER_SIZE);
  assert(Y2K_SIZE==BUFFER_SIZE);
  
  parse_options(&my_data, argc, argv);
  install_mbr(&my_data);
  return 0;
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-file-offsets: ((substatement-open . 0)) */
/* End: */
