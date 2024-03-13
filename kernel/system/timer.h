#ifndef KERNEL_SYSTEM_TIMER
#define KERNEL_SYSTEM_TIMER

#include <stdint.h>

#include "kernel/include/list.h"

#define TIMER_MAGIC 0x4b87ad6e

#define TIMER_INITIALIZER(_callback, _expires) \
{                                        \
  .callback = (_callback),               \
  .expires = (_expires),                 \
  .magic = TIMER_MAGIC                   \
}

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

struct sleep_timer {
	uint64_t expires;
	void (*callback)(struct sleep_timer *);
	struct list_head sibling;
	// TODO: MQ 2020-07-02 If there is the issue, which timer is deleted and iterated, considering using lock (better mutex)
	// spinlock_t lock;
	uint32_t magic;
};

void add_timer(struct sleep_timer *timer);
void del_timer(struct sleep_timer *timer);
void mod_timer(struct sleep_timer *timer, uint64_t expires);
bool is_actived_timer(struct sleep_timer *timer);
void timer_init();

#endif