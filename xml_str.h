#ifndef _XML_STR_H
#define	_XML_STR_H

const wchar_t *skip_space(const wchar_t *data, const wchar_t *data_end);
int str_issapce(wchar_t ch);
int strlen_t(const wchar_t *c, const wchar_t *end, const wchar_t *termi);
int strcpy_t(wchar_t *c, const wchar_t *src, const wchar_t *termi);
int str_count(const wchar_t *src, const wchar_t *end, int ch, int term1, int term2);


#endif // !_XML_ASSIST_H
