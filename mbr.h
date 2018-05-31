#ifndef MBR_H
#define MBR_H

/* This file defines the interface to the encapsulated binary file
   produced by ld. */

extern u_int8_t _binary_mbr_b_start[];
extern u_int8_t _binary_mbr_b_end[];

#define MBR_START _binary_mbr_b_start
#define MBR_END _binary_mbr_b_end
#define MBR_SIZE (_binary_mbr_b_end - _binary_mbr_b_start)

extern u_int8_t _binary_y2k_b_start[];
extern u_int8_t _binary_y2k_b_end[];

#define Y2K_START _binary_y2k_b_start
#define Y2K_END _binary_y2k_b_end
#define Y2K_SIZE (_binary_y2k_b_end - _binary_y2k_b_start)

/* This defines the format of the parameters in the MBR for versions 0
   and 1.  This structure is now frozen. */
struct mbr_params_v1
{
  u_int8_t sig[6]; /* This must be at the top */
#define MP_V1_SIG "NDTmbr"
  u_int8_t mp_reserved1;
  u_int8_t ver_compat;
#define MP_V1_VERSION 2 /* A bit silly really. */
  u_int8_t variant;
  u_int8_t version;
#define MP_VARIANT_Y2K 1
#define MP_VERSION_Y2K 1 /* The version at which it was introduced. */
  u_int8_t flags;
#define MP_FLAG_EN1 1
#define MP_FLAG_EN2 2
#define MP_FLAG_EN3 4
#define MP_FLAG_EN4 8
#define MP_FLAG_ENF 16
#define MP_FLAG_ENA 128
  u_int8_t deflt;
#define MP_DEFLT_BITS 7
#define MP_DEFLT_ISHIFT 64
#define MP_DEFLT_IKEY 128
  union
  {
#define MP_V1_FMTB_VER 2
    struct
    {
      u_int8_t delay;
#define MP_V1_DELAY_INT 0xff
      u_int8_t drive;
#define MP_DRIVE_UNSET 0xff
    } fmt_a;
    struct
    {
      u_int8_t delay[2];
      u_int8_t drive;
    } fmt_b;
  } u;
};

/* This defines the version 2 parameters.  It must match the code
   (mbr.S).  Everything uses u_int8_t so that packing will work
   (unless something strange happens).  Our 16 bit fields are made up
   of pairs of 8 bit fields.  This gets round endian problems too (we
   actually support running the installer on the wrong hardware).  The
   structure sits just before the partition table.  As such, it starts
   at the end and goes backwards. */
struct mbr_params_v2
{
  u_int8_t date[3]; /* year, month in BCD. */
  u_int8_t flags;
  u_int8_t deflt;
#define MP_V2_DELAY_INT 0xffff
  u_int8_t delay[2];
  u_int8_t drive;
  u_int8_t variant;
#define MP_V2_VERSION_MIN 2 /* The lowest valid version number. */
#define MP_V2_VERSION 2 /* Current version. */
  u_int8_t ver_compat; /* Versions 0,1 are illegal here. */
  u_int8_t version;    /* Versions 0,1 are illegal here. */
  u_int8_t sig[2];
  /* The signature has the following properties:
   *   It was generated randomly.
   *   It does not match sensible i386 instructions.
   *   It is an invalid parameter pointer for the V1 layout which was
   *     in this location.
   */
#define MP_V2_SIG {0xef,0x17}
};

#endif
