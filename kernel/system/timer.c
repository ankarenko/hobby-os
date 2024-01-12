#include "kernel/cpu/hal.h"
#include "kernel/system/time.h"
#include "kernel/util/debug.h"

#include "kernel/system/timer.h"

static struct list_head list_of_timer;

static void assert_timer_valid(struct sleep_timer *timer) {
	if (timer->magic != TIMER_MAGIC) {
    assert_not_reached();
  }
}

void add_timer(struct sleep_timer *timer) {
  assert_timer_valid(timer);
  list_add_tail(&timer->sibling, &list_of_timer);
  
  /*
  struct sleep_timer *iter, *node = NULL;
	// to make it sorted
  list_for_each_entry(iter, &list_of_timer, sibling) {
		assert_timer_valid(iter);
		if (timer->expires < iter->expires)
			break;
		node = iter;
	}

	if (node)
		list_add(&timer->sibling, &node->sibling);
	else
		list_add_tail(&timer->sibling, &list_of_timer);
  */
}

void del_timer(struct sleep_timer *timer) {
  list_del(&timer->sibling);
}

void mod_timer(struct sleep_timer *timer, uint64_t expires) {
  del_timer(timer);
  timer->expires = expires;
  add_timer(timer);
}

bool is_actived_timer(struct sleep_timer *timer) {
  return timer->sibling.prev != LIST_POISON1 && timer->sibling.next != LIST_POISON2;
}

static int32_t timer_schedule_handler(interrupt_registers *regs) {
  struct sleep_timer *iter, *next;
	uint64_t cms = get_seconds(NULL) * 1000;
  
  // should be safe because callback most probably will remove timer from the list
  list_for_each_entry_safe(iter, next, &list_of_timer, sibling) {
		assert_timer_valid(iter);
    
		if (iter->expires <= cms)
			iter->callback(iter);
	}

	return IRQ_HANDLER_CONTINUE;
}

void timer_init() {
  INIT_LIST_HEAD(&list_of_timer);
  register_interrupt_handler(IRQ8, timer_schedule_handler);
}