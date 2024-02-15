#include "kernel/memory/malloc.h"

#include "kernel/util/circular_buffer.h"

// The definition of our circular buffer structure is hidden from the user
struct circular_buf_t {
  char *buffer;
  size_t head;
  size_t tail;
  size_t max;  // of the buffer
  bool full;
};

// reading from tail to head [ tail -> head ]
static void advance_pointer(struct circular_buf_t *cbuf) {
  if (cbuf->full) {
    cbuf->tail = (cbuf->tail + 1) % cbuf->max;
  }

  cbuf->head = (cbuf->head + 1) % cbuf->max;

  cbuf->full = (cbuf->head == cbuf->tail);
}

static void retreat_pointer(struct circular_buf_t *cbuf) {
	cbuf->full = false;
	cbuf->tail = (cbuf->tail + 1) % cbuf->max;
}

struct circular_buf_t *circular_buf_init(char *buffer, size_t size) {
  struct circular_buf_t *cbuf = kcalloc(1, sizeof(struct circular_buf_t));

  cbuf->buffer = buffer;
  cbuf->max = size;
  circular_buf_reset(cbuf);

  return cbuf;
}

void circular_buf_free(struct circular_buf_t *cbuf) {
  kfree(cbuf->buffer);
}

void circular_buf_reset(struct circular_buf_t *cbuf) {
  cbuf->full = false;
  cbuf->head = 0;
  cbuf->tail = 0;
}

void circular_buf_put(struct circular_buf_t *cbuf, char data) {
  cbuf->buffer[cbuf->head] = data;

  advance_pointer(cbuf);
}

int circular_buf_put2(struct circular_buf_t *cbuf, char data) {
  int r = -1;

	if (!circular_buf_full(cbuf)) {
		cbuf->buffer[cbuf->head] = data;
		advance_pointer(cbuf);
		r = 0;
	}

	return r;
}

int circular_buf_get(struct circular_buf_t *cbuf, char *data) {
  int r = -1;

	if (!circular_buf_empty(cbuf)) {
		*data = cbuf->buffer[cbuf->tail];
		retreat_pointer(cbuf);

		r = 0;
	}

	return r;
}

bool circular_buf_empty(struct circular_buf_t *cbuf) {
  return (!cbuf->full && (cbuf->head == cbuf->tail));
}

bool circular_buf_full(struct circular_buf_t *cbuf) {
  return cbuf->full;
}

size_t circular_buf_capacity(struct circular_buf_t *cbuf) {
  return cbuf->max;
}

size_t circular_buf_size(struct circular_buf_t *cbuf) {
  size_t size = cbuf->max;
  if (cbuf->full)
    return cbuf->max;

  return cbuf->head >= cbuf->tail ? 
    cbuf->head - cbuf->tail : cbuf->max + cbuf->head - cbuf->tail;
}
