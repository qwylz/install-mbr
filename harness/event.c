#include "harness.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/*#define DEBUG_EVENT */

enum event_type { KEY, SHIFT, WAIT, RATE };

typedef struct event
{
  struct event *next;
  enum event_type type;
  unsigned long value;
} event;

typedef struct
{
  event *head;
  event **tail;
} event_queue_t;

static event_queue_t event_queue = {0};
static event_queue_t key_event_queue = {0};
static event *wait_event = NULL;
static int event_debugging = 0;

static event *queue_pop(event_queue_t *q)
{
  event *ev = q->head;
  if (ev)
  {
    q->head = ev->next;
    if (q->tail == &(ev->next))
      q->tail = &(q->head);
    ev->next = NULL;
  }
#ifdef DEBUG_EVENT
  fprintf(stderr, "Removing event %p from queue %p\n", ev, q);
#endif
  return ev;
}

static void queue_push(event_queue_t *q, event *ev)
{
  assert (ev->next == NULL);
#ifdef DEBUG_EVENT
  fprintf(stderr, "Adding event %p to queue %p\n", ev, q);
#endif
  *(q->tail?q->tail:&(q->head)) = ev;
  q->tail = &(ev->next);
}

unsigned long peek_key_event(void)
{
  return key_event_queue.head ? key_event_queue.head->value : NO_KEY;
}

unsigned long get_key_event(void)
{
  /* This is a blocking routine. */
  unsigned long result = NO_KEY;
  while (1)
  {
    /* Try keyboard buffer. */
    event *ev = queue_pop(&key_event_queue);
    if (ev)
    {
      result = ev->value;
#ifdef DEBUG_EVENT
      fprintf(stderr, "%d:Freeing event: %p\n", __LINE__, ev);
#endif
      free(ev);
      return result;
    }
    /* Ok, no joy there.  Try the time warp. */
    if (!wait_event)
    {
      /* We're deadlocked. */
      outputf("Keyboard buffer empty!\n");
      exit(0);
    }
    warp_time (wait_event->value);
  }

  return result;
}

static void process_events(void)
{
  while(1)
  {
    event *ev = queue_pop(&event_queue);
    if (!ev)
    {
      /* Report an empty event queue. */
      outputf("Event list empty.\n");
      return;
    }

    if (event_debugging)
      outputf("Processing event of type %d, value %lx\n",
	      ev->type, ev->value);

    switch (ev->type)
    {
    case WAIT:
      wait_event = ev;
      return;

    case KEY:
      /* Just put it in the keyboard buffer. */
      queue_push (&key_event_queue, ev);
      break;

    case SHIFT:
      set_shift_state (ev->value);
#ifdef DEBUG_EVENT
      fprintf(stderr, "%d:Freeing event: %p\n", __LINE__, ev);
#endif
      free(ev);
      break;
      
    case RATE:
      set_rate (ev->value);
#ifdef DEBUG_EVENT
      fprintf(stderr, "%d:Freeing event: %p\n", __LINE__, ev);
#endif
      free(ev);
      break;

    default:
      fprintf(stderr, "Invalid event type: %d\n", ev->type);
      exit(1);
    }
  }
}

void process_events_at(unsigned long t)
{
  if (event_debugging)
    outputf("Processing events at %ld\n", t);

  while (wait_event && wait_event->value == t)
  {
#ifdef DEBUG_EVENT
    fprintf(stderr, "%d:Freeing event: %p\n", __LINE__, wait_event);
#endif
    free(wait_event);
    wait_event = NULL;
    process_events();
  }
}

static event *new_event(void)
{
  event *ev = malloc(sizeof(*ev));
#ifdef DEBUG_EVENT
  fprintf(stderr, "Allocated event at %p\n", ev);
#endif
  if (!ev)
  {
    fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
    exit(1);
  }
  ev->next = NULL;
  return ev;
}

static void queue_new_event(event_queue_t *queue, enum event_type t,
			    unsigned long value)
{
  event *ev = new_event();
  ev->type = t;
  ev->value = value;
  queue_push(queue, ev);
}

void put_wait_event(unsigned long t)
{
  event *ev = new_event();
  ev->type = WAIT;
  ev->value = t;
  if (!wait_event)
    wait_event = ev;
  else
    queue_push(&event_queue, ev);
}

void put_key_event(unsigned long key)
{
  queue_new_event(wait_event ? &event_queue : &key_event_queue, KEY, key);
}

void put_shift_event(unsigned long shift)
{
  if (wait_event)
    queue_new_event(&event_queue, SHIFT, shift);
  else
    set_shift_state (shift);
}

void put_rate_event(unsigned long rate)
{
  if (wait_event)
    queue_new_event(&event_queue, RATE, rate);
  else
    set_rate (rate);
}

void event_debug()
{
  event_debugging = 1;
}

/*  */
/* Local Variables: */
/* mode:c */
/* c-indentation-style: "whitesmith" */
/* c-basic-offset: 2 */
/* End: */
