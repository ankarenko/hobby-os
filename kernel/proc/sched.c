#include <stdint.h>
#include "../cpu/hal.h"

static volatile uint32_t scheduler_lock_counter = 0;

void lock_scheduler() {
	disable_interrupts();
	scheduler_lock_counter++;
}

void unlock_scheduler() {
	scheduler_lock_counter--;
	if (scheduler_lock_counter == 0)
		enable_interrupts();
}