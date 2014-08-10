#ifndef _XML_H
#define	_XML_H

enum xml_type {
        XML_ROOT,
        XML_COMMENT,
        XML_ELEMENT,
        XML_ELEMENT_SELF,
};

struct xml_element;

struct xml_element *xml_load_file(const wchar_t *path);

struct xml_element *xml_new(const wchar_t *name, const wchar_t *value, enum xml_type type);
int xml_free_child(struct xml_element *tree);
int xml_free(struct xml_element *tree);

const wchar_t *xml_get_name(const struct xml_element *node);
const wchar_t *xml_get_value(const struct xml_element *node);

wchar_t *xml_set_value(struct xml_element *node, const wchar_t *value);

struct xml_element *xml_walkdown(const struct xml_element *node);
struct xml_element *xml_walkup(const struct xml_element *node);
struct xml_element *xml_walknext(const struct xml_element *node);
struct xml_element *xml_walkprev(const struct xml_element *node);

struct xml_element *xml_search_child(struct xml_element *parent, const wchar_t *name);
struct xml_element *xml_search_brother(struct xml_element *brother, const wchar_t *name);


struct xml_element *xml_append_child(struct xml_element *parent, struct xml_element *child);
struct xml_element *xml_append_brother(struct xml_element *b1, struct xml_element *b2);


int xml_need_len(const struct xml_element *tree);
int xml_save_data(const struct xml_element *tree, wchar_t *buff, unsigned long cnt);

#endif // !_XML_H

