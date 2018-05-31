#ifndef HARNESS_H
#define HARNESS_H

#ifdef __GNUC__
#define ATTRIBUTE(X) __attribute__(X)
#else
#define ATTRIBUTE(X)
#endif

struct vm86plus_struct;

/*** args.c ***/

extern void args_init(int ac, char *av[]);

/*** bios.c ***/

#define INTERFACE_BIG_DISK   ((unsigned int)1)

extern void bios_init(void);
extern void set_interfaces(unsigned int enable, unsigned int mask);
extern void set_date(unsigned int y, unsigned int m, unsigned int d);
extern int handle_int_10(struct vm86plus_struct *v86);
extern int handle_int_13(struct vm86plus_struct *v86);
extern int handle_int_16(struct vm86plus_struct *v86);
extern int handle_int_1a(struct vm86plus_struct *v86);
extern void handle_int_e4(struct vm86plus_struct *v86);
extern void set_shift_state (unsigned long);

/*** event.c ***/

#define NO_KEY (~(unsigned long)0)

/* Initialisation */
extern void init_event_time(unsigned long);
extern void put_wait_event(unsigned long);
extern void put_key_event(unsigned long);
extern void put_shift_event(unsigned long);
extern void put_rate_event(unsigned long);
extern void event_debug(void);

/* Event processing */
extern unsigned long peek_key_event(void);
extern unsigned long get_key_event(void);
extern void process_events_at(unsigned long);

/*** mbr.c ***/

extern void set_mbr(const char *);
extern void mbr_init(void);
extern void compare_mbr(const unsigned char *);

/*** output.c ***/

/* This is like printf, but cooperates with outputc. */
extern void outputf(const char *format, ...) ATTRIBUTE((format(printf, 1, 2)));

/* This writes a character in a printable but compact form.  Multiple
   writes are written on a single line. */
extern void outputc(char c);

/*** process.c ***/

extern void process_init(void);
extern void set_cpu_limit(unsigned long time);

/*** time.c ***/

extern void init_time(unsigned long t);
extern void set_rate(unsigned long rate);
extern unsigned long get_time(void);
extern void tick(void);
extern void warp_time(unsigned long t);

/*** v86.c ***/

extern void vm86_run(void);
extern void set_pc(unsigned long cs, unsigned long ip);
extern void vm86_debug(void);
extern void vm86_step(void);

#endif
