/* harness for testing a master boot record

   Copyright (C) 2001 Neil Turton.

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

/* The purpose of this program is to provide an execution environment
   similar to the BIOS for a MBR, which doesn't depend on DOSEMU
   (which keeps changing and doesn't provide the control we need).  It
   should provide results back to the MBR in response to BIOS calls,
   according to command line arguments. 

   We need to specify:
     1. The MBR to run.  This is a 512-byte file. (-f <file>)
     2. The initial time of day. (-t <time>.<sub>)
     3. Schedule a key-press event (-k <key>)
     4. Wait for a particular time (-w <time>.<sub>)
     5. A timer rate. (-r <rate>)
     6. Real time limit (-a <secs>)
*/

#include "harness.h"

/*** Top level ***********************************************************/

int main(int argc, char **argv)
{
  process_init();
  bios_init();
  args_init(argc, argv);
  mbr_init();
  vm86_run();
  return 0;
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
