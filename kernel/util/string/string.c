#include "kernel/memory/malloc.h"

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

int32_t strlsplat(const char *s1, int32_t pos, char **sf, char **sl) {
	if (pos < 0)
		return -1;

	size_t length = strlen(s1);
	if (pos && sf) {
		*sf = kcalloc(pos + 1, sizeof(char));
		memcpy(*sf, s1, pos);
	}
	if (pos < (int32_t)length && sl) {
		*sl = kcalloc(length - pos, sizeof(char));
		memcpy(*sl, s1 + pos + 1, length - 1 - pos);
	}
	return 0;
}

int32_t strliof(const char *s1, const char *s2) {
	const char *s = strrstr(s1, s2);
	if (s)
		return s - s1;
	else
		return -1;
}