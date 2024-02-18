#include "kernel/proc/wait.h"

void wake_up(struct wait_queue_head *hq) {
	struct wait_queue_entry *iter, *next;
	list_for_each_entry_safe(iter, next, &hq->list, sibling) {
    assert(iter->callback);
		iter->callback(iter->th);
	}
}