#include <assert.h>
#include "xml_str.h"

int str_issapce(wchar_t ch)
{
	return (ch== L'\t' || ch == L'\r' || ch == L'\n' || ch == L' ');
}


const wchar_t *skip_space(const wchar_t *data, const wchar_t *data_end)
{
	assert(data);
	assert(data_end);

	while (str_issapce(*data))
		data++;

	return data;
}

int strlen_t(const wchar_t *c, const wchar_t *end, const wchar_t *termi)
{
	const wchar_t *t;
	const wchar_t *org;

	org = c;
	while (c < end) {
		t = termi;
		while (*t != 0) {
			if (*t == *c)
				goto end;

			t++;
		}
		c++;
	}

end:
	return c - org;
}

int strcpy_t(wchar_t *c, const wchar_t *src, const wchar_t *termi)
{
	const wchar_t *t;
	while (1) {
		t = termi;
		while (*t != 0) {
			if (*t == *src)
				goto end;
			t++;
		}
		*c++ = *src++;
	}
end:
	*c = 0;

	return 0;
}

int str_count(const wchar_t *src, const wchar_t *end, int ch, int term1, int term2)
{
	int cnt;

	cnt = 0;
	while (*src != term1 && *src != term2 && src < end) {
		if (*src == ch)
			cnt++;
		src++;
	}

	return cnt;
}