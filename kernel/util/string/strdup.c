#include "kernel/memory/malloc.h"

#include "kernel/util/string/string.h"

char *strdup(const char *s) {
	size_t len = strlen(s) + 1;
	void *new = kcalloc(len, sizeof(char));

	if (new == NULL)
		return NULL;

	return (char *)memcpy(new, s, len);
}