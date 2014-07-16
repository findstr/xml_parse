#ifndef _XML_H
#define	_XML_H

struct xml_element;

struct xml_element *xml_load_file(const wchar_t *path);
int xml_free(struct xml_element *tree);
const wchar_t *xml_get_name(const struct xml_element *node);
const wchar_t *xml_get_value(const struct xml_element *node);
const struct xml_element *xml_walkdown(const struct xml_element *node);
const struct xml_element *xml_walkup(const struct xml_element *node);
const struct xml_element *xml_walknext(const struct xml_element *node);
const struct xml_element *xml_walkprev(const struct xml_element *node);


#endif // !_XML_H

