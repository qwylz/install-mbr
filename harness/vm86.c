#include <asm/unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "vm86.h"
#include "harness.h"

static struct vm86plus_struct v86 = {};
static int v86_debugging = 0;

/* The purpose of this file is to interact with the kernel and run the
   V86 task.  */

/* Some versions of libc don't have vm86, so call the system call
 * directly. */
static int vm86(unsigned long fn, struct vm86plus_struct *v86)
{
  int res;
  __asm__ __volatile__("int $0x80" :
		       "=a" (res) :
		       "0" (__NR_vm86),
		       "b" (fn),
		       "c" (v86) :
		       "memory");
  if(res < 0) {
    errno = -res;
    return -1;
  }
  return res;
}

static void vm86_dump(void)
{
  unsigned long eflags = v86.regs.eflags;
  /* Format:
   * eax:00000000 ebx:00000000 ecx:00000000 edx:00000000 PC:0000:00000000
   * esi:00000000 edi:00000000 ebp:00000000 efl:00000000 SP:0000:00000000
   * ds:0000 es:0000 fs:0000 gs:0000
   */
  outputf("eax:%08lx ebx:%08lx ecx:%08lx edx:%08lx PC:%04x:%08lx\n",
	  v86.regs.eax, v86.regs.ebx, v86.regs.ecx, v86.regs.edx,
	  v86.regs.cs, v86.regs.eip);
  outputf("esi:%08lx edi:%08lx ebp:%08lx efl:%08lx SP:%04x:%08lx\n",
	  v86.regs.esi, v86.regs.edi, v86.regs.ebp, eflags,
	  v86.regs.ss, v86.regs.esp);
  outputf("ds:%04x es:%04x fs:%04x gs:%04x",
	  v86.regs.ds, v86.regs.es, v86.regs.fs, v86.regs.gs);
  outputf(" FLAGS: %s %s %s %s %s %s %s %s %s\n",
	  (eflags & CF ? "C" : "c"),
	  (eflags & PF ? "P" : "p"),
	  (eflags & AF ? "A" : "a"),
	  (eflags & ZF ? "Z" : "z"),
	  (eflags & SF ? "S" : "s"),
	  (eflags & TF ? "T" : "t"),
	  (eflags & IF ? "I" : "i"),
	  (eflags & DF ? "D" : "d"),
	  (eflags & OF ? "O" : "o"));
}

static void segv_handler(int i)
{
  outputf("SIGSEGV recieved.\n");
  vm86_dump();
}

void vm86_run()
{
  int i;

  v86.cpu_type = CPU_386;
  for(i=8; i--; )
  {
    v86.int_revectored.__map[i] = ~(unsigned long)0;
    v86.int21_revectored.__map[i] = ~(unsigned long)0;
  }

  while (1)
  {
    int res;
    struct sigaction old_segv, new_segv = {};

    if (v86_debugging)
    {
      outputf("Entering VM86 mode at %04x:%04lx.\n",
	      v86.regs.cs, v86.regs.eip);
      vm86_dump();
    }

    new_segv.sa_handler = &segv_handler;
    new_segv.sa_flags = SA_ONESHOT | SA_NOMASK;
    sigaction(SIGSEGV, &new_segv, &old_segv);
    res = vm86(VM86_ENTER, &v86);
    if (res == -1) {
      perror("vm86");
      exit(1);
    }
    sigaction(SIGSEGV, &old_segv, NULL);

    if (v86_debugging)
    {
      char *event_str;
      switch (VM86_TYPE(res))
      {
      case VM86_SIGNAL:    event_str="signal";  break;
      case VM86_UNKNOWN:   event_str="fault";   break;
      case VM86_INTx:      event_str="int";     break;
      case VM86_STI:       event_str="sti";     break;
      case VM86_PICRETURN: event_str="pic";     break;
      case VM86_TRAP:      event_str="trap";    break;
      default:             event_str="unknown"; break;
      }
      outputf("Processing vm86 event %d (%s), code 0x%x at %04x:%04lx\n",
	      VM86_TYPE(res), event_str, VM86_ARG(res),
	      v86.regs.cs, v86.regs.eip);
      vm86_dump();
    }

    switch (VM86_TYPE(res))
    {
    case VM86_INTx:
      tick();
      switch (VM86_ARG(res))
      {
      case 0x10:
	if (handle_int_10(&v86))
	  continue;
	break;

      case 0x13:
	if (handle_int_13(&v86))
	  continue;
	break;

      case 0x16:
	if (handle_int_16(&v86))
	  continue;
	break;

      case 0x1a:
	if (handle_int_1a(&v86))
	  continue;
	break;

      case 0xf2:
	continue;

      case 0xe4: /* Exit */
	handle_int_e4(&v86);
	vm86_dump();
	return;
      }
      outputf("INT 0x%x at %04x:%04lx\n", VM86_ARG(res),
	      v86.regs.cs, v86.regs.eip);
      vm86_dump();
      return;

    case VM86_TRAP:
      continue;

    default:
      outputf("Unexpected event %d, code 0x%x at %04x:%04lx\n",
	      VM86_TYPE(res), VM86_ARG(res), v86.regs.cs, v86.regs.eip);
      return;
    }
  }
}

void set_pc(unsigned long cs, unsigned long ip)
{
  v86.regs.cs = cs;
  v86.regs.eip = ip;
}

void vm86_debug()
{
  v86_debugging = 1;
}

void vm86_step()
{
  v86.regs.eflags |= TF;
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
