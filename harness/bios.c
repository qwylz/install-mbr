#include "harness.h"
#include "vm86.h"
#include <string.h>

typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef unsigned long      DWORD;
typedef unsigned long long QWORD;

typedef struct
{
  BYTE  size;
  BYTE  reserved;
  WORD  count;
  WORD  addr;
  WORD  seg;
  QWORD block;
} disk_address_packet;

#define EFLAGS(v86) ((v86)->regs.eflags)

#define AX(v86) ((unsigned short)((v86)->regs.eax))
#define AL(v86) ((unsigned char)((v86)->regs.eax))
#define AH(v86) ((unsigned char)((v86)->regs.eax >> 8))
#define SET_AL(v86, new_al)      \
  do {                           \
    (v86)->regs.eax &= ~0xff;    \
    (v86)->regs.eax |= (new_al); \
  } while(0)
#define SET_AH(v86, new_ah)           \
  do {                                \
    (v86)->regs.eax &= ~0xff00;       \
    (v86)->regs.eax |= (new_ah) << 8; \
  } while(0)
#define SET_AX(v86, new_ax)      \
  do {                           \
    (v86)->regs.eax &= ~0xffff;  \
    (v86)->regs.eax |= (new_ax); \
  } while(0)

#define BX(v86) ((unsigned short)((v86)->regs.ebx))
#define SET_BX(v86, new_bx)      \
  do {                           \
    (v86)->regs.ebx &= ~0xffff;  \
    (v86)->regs.ebx |= (new_bx); \
  } while(0)

#define ECX(v86) ((v86)->regs.ecx)
#define CX(v86) ((unsigned short)((v86)->regs.ecx))
#define CL(v86) ((unsigned char)((v86)->regs.ecx))
#define CH(v86) ((unsigned char)((v86)->regs.ecx >> 8))
#define SET_CX(v86, new_cx)      \
  do {                           \
    (v86)->regs.ecx &= ~0xffff;  \
    (v86)->regs.ecx |= (new_cx); \
  } while(0)

#define EDX(v86) ((v86)->regs.edx)
#define DX(v86) ((unsigned short)((v86)->regs.edx))
#define DL(v86) ((unsigned char)((v86)->regs.edx))
#define DH(v86) ((unsigned char)((v86)->regs.edx >> 8))
#define SET_DH(v86, new_dh)           \
  do {                                \
    (v86)->regs.edx &= ~0xff00;       \
    (v86)->regs.edx |= (new_dh) << 8; \
  } while(0)
#define SET_DX(v86, new_dx)      \
  do {                           \
    (v86)->regs.edx &= ~0xffff;  \
    (v86)->regs.edx |= (new_dx); \
  } while(0)

#define SI(v86) ((unsigned short)((v86)->regs.esi))

#define CS(v86) ((v86)->regs.cs)
#define DS(v86) ((v86)->regs.ds)
#define ES(v86) ((v86)->regs.es)

#define EBP(v86) ((v86)->regs.ebp)
#define BP(v86) ((unsigned short)((v86)->regs.ebp))

#define EIP(v86) ((v86)->regs.eip)

#define FAR(seg, off) ((void*)(16*(unsigned long)(seg)+(off)))

static unsigned int interfaces_enabled = 0;
static unsigned int current_year = 2001, current_month = 1, current_day = 1;

void bios_init()
{
  set_shift_state(0);
}

void set_interfaces(unsigned int enable, unsigned int mask)
{
  interfaces_enabled = (interfaces_enabled & mask) | enable;
}

void set_date(unsigned int y, unsigned int m, unsigned int d)
{
  current_year = y;
  current_month = m;
  current_day = d;
}

int handle_int_10(struct vm86plus_struct *v86)
{
  switch (AH(v86))
  {
  case 0x0e: /* Character output */
    outputc(AL(v86));
    return 1;
  }
  return 0;
}

static void read_sector(unsigned char *data, DWORD location_lo,
			DWORD location_hi)
{
  /* 0xe4 is inb - should abort if executed. */
  memset(data, 0xe4, 512);
  *(WORD*)(data+  0) = 0xe4cd; /* int 0xe4 */
  *(DWORD*)(data+ 2) = 0xdecea5ed;
  *(DWORD*)(data+ 6) = location_lo ^ 0x5569aa96;
  *(DWORD*)(data+10) = location_hi ^ 0x3ca55ac3;
  *(WORD*)(data+510) = 0xaa55; /* Signature */
}

/* Handle an INT_13 event.  Return true if it is handled. */
int handle_int_13(struct vm86plus_struct *v86)
{
  unsigned int drive = DL(v86);
  unsigned int head = DH(v86);
  unsigned int cylinder = CH(v86) | (((int)CL(v86) & 0xc0) << 2);
  unsigned int sector = CL(v86) & 0x3f;
  
  /* Clear the error flag for most cases. */
  EFLAGS(v86) &= ~CF;

  /* Check the sub-function. */
  switch (AH(v86))
  {
  case 0x2: /* Disk read. */
    if (AL(v86) != 1) /* Only allow 1 sector. */
      break;
    outputf("Read sector d%d h%d c%d s%d to %04x:%04x\n", drive, head,
	    cylinder, sector, ES(v86), BX(v86));
    read_sector(FAR(ES(v86), BX(v86)),
		(sector<<0)|(cylinder<<16),
		(head<<0)|(drive<<16));
    return 1;

  case 0x3: /* Disk write. */
    if (AL(v86) != 1) /* Only allow 1 sector. */
      break;
    outputf("Write sector d%d h%d c%d s%d to %04x:%04x\n", drive, head,
	    cylinder, sector, ES(v86), BX(v86));
    compare_mbr(FAR(ES(v86), BX(v86)));
    return 1;

  case 0x41: /* Big disk installation check. */
    /* Make sure the parameters are initialised. */
    if (BX(v86)!=0x55aa)
      break;

    /* Only support hard disks. */
    if (0x80&~DL(v86))
      break;

    /* Is the interface enabled? */
    if (interfaces_enabled & INTERFACE_BIG_DISK)
    {
      /* Yes.  We support this. */
      SET_BX(v86, 0xaa55);
      SET_AH(v86, 0x01); /* Major version. */
      SET_AL(v86, 0x95); /* Random number. */
      SET_CX(v86, 1);    /* Only support functions 0x42-0x44,0x47,0x48. */
      SET_DH(v86, 0);    /* Extension version. */
      /* FIXME: Could have bad support bitmap.  Could change version
         numbers. */
    }
    else
    {
      /* No.  We don't support this. */
      EFLAGS(v86) |= CF;
      SET_AH(v86, 0x1);
    }
    return 1;

  case 0x42: /* Extended read. */
  {
    disk_address_packet *pkt;

    /* Abort if this interface isn't enabled. */
    if (INTERFACE_BIG_DISK & ~interfaces_enabled)
      break;

    /* Only support hard disks. */
    if (0x80&~DL(v86))
      break;

    /* Get a pointer to the parameters. */
    pkt = FAR(DS(v86), SI(v86));

    /* The size must be correct. */
    if (pkt->size != sizeof(*pkt) || pkt->reserved != 0 ||
	pkt->count != 1)
      break;

    outputf("Read sector #0x%08x%08x to %04x:%04x\n",
	    (int)(pkt->block>>32), (int)(pkt->block),
	    pkt->seg, pkt->addr);

    /* Fill in the data. */
    read_sector(FAR(pkt->seg, pkt->addr), pkt->block, pkt->block>>32);

    /* FIXME: Could return error. */
    EFLAGS(v86) &= ~CF;
    SET_AH(v86, 0);
    return 1;
  }

  case 0x43: /* Extended write. */
    break; /* Not supported. */
  case 0x44: /* Verify sectors. */
    break; /* Not supported. */
  case 0x47: /* Extended seek. */
    break; /* Not supported. */
  case 0x48: /* Get drive parameters. */
    break; /* Not supported. */
  }
  return 0;
}

int handle_int_16(struct vm86plus_struct *v86)
{
  unsigned long key;

  switch (AH(v86))
  {
  case 0: /* Get key stroke */
    SET_AX(v86, get_key_event());
    return 1;

  case 1: /* Check for key stroke */
    key = peek_key_event();
    if (key == NO_KEY)
    {
      EFLAGS(v86) |= ZF;
    }
    else
    {
      EFLAGS(v86) &= ~(unsigned long)ZF;
      SET_AX(v86, key);
    }
    return 1;
  }
  return 0;
}

int handle_int_1a(struct vm86plus_struct *v86)
{
  unsigned int time;

  switch (AH(v86))
  {
  case 0: /* Get time */
    time = get_time();
    SET_CX(v86, (time>>16) & 0xffff);
    SET_DX(v86, (time>>0)  & 0xffff);
    return 1;
  }
  return 0;
}

static void dump_partition(DWORD seg, DWORD offset)
{
  BYTE *data = FAR(seg, offset);
  /* Format:
   *   0   BYTE   Active flag.
   *   1   BYTE   Start head (DH).
   *   2   BYTE   Start sector + track high bits (CL).
   *   3   BYTE   Start track (CH).
   *   4   BYTE   Partition Type
   *   5   BYTE   End head (DH).
   *   6   BYTE   End sector + track high bits (CL).
   *   7   BYTE   End track (CH).
   *   8   DWORD  Sectors preceeding partition.
   *   12  DWORD  Partition length.
   */
  outputf("Partition: %sactive  type: 0x%02x\n",
	  data[0] == 0 ? "in" :
	  data[0] == 0x80 ? "" :
	  "maybe ",
	  data[4]);
  outputf("Start: C=%d H=%d S=%d  End: C=%d H=%d S=%d\n",
	  data[3] | (data[2]&0xc0)*4,
	  data[1],
	  data[2] & 0x3f,
	  data[7] | (data[6]&0xc0)*4,
	  data[5],
	  data[6] & 0x3f);
  outputf("Sector 0x%08lx  Length 0x%08lx\n",
	  ((DWORD*)data)[2],
	  ((DWORD*)data)[3]);
}

void handle_int_e4(struct vm86plus_struct *v86)
{
  DWORD *data = FAR(CS(v86), EIP(v86));
  DWORD location_lo = data[1]^0x5569aa96;
  DWORD location_hi = data[2]^0x3ca55ac3;

  if (data[0] != 0xdecea5ed)
    return;

  outputf("Exit: %04lx %04lx %04lx %04lx\n",
	  location_hi>>16, location_hi&0xffff,
	  location_lo>>16, location_lo&0xffff);

  if ( DL(v86) & 0x80 )
  {
    /* I forgot segment register gets used here and I can't find a
     * helpful spec.  I think it's ES, but I could be wrong.  Both
     * should be the same. */
    dump_partition(ES(v86), SI(v86));
    if ( DS(v86) != ES(v86) )
    {
      outputf("Mismatched segment registers.\n");
    }
  }
  else
  {
    outputf("Floppy boot.\n");
  }
}

void set_shift_state (unsigned long state)
{
  unsigned char *flag = (void*)0x417;
  *flag = state;
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
