/* https://en.wikipedia.org/wiki/Code_page_437 */
#include "tagspriv.h"

int
cp437toutf8(char *o, int osz, const char *s, int sz)
{
	/* FIXME somebody come up with portable code */
	return snprint(o, osz, "%.*s", sz, s);
}
