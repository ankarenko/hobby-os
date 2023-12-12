#include "kernel/util/string/string.h"

char *strrstr(char *string, char *find) {
	size_t stringlen, findlen;
	char *cp;

	findlen = strlen(find);
	stringlen = strlen(string);
	if (findlen > stringlen)
		return NULL;

	for (cp = string + stringlen - findlen; cp >= string; cp--)
		if (strncmp(cp, find, findlen) == 0)
			return cp;

	return NULL;
}

int32_t strliof(const char *s1, const char *s2) {
	const char *s = strrstr(s1, s2);
	if (s)
		return s - s1;
	else
		return -1;
}