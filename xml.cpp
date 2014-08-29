#include <assert.h>
#include <wchar.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "array.h"
#include "xml_str.h"
#include "xml.h"

#define	 XML_SPACE_STR	L"\r\n \t"

struct xml_attr {
	wchar_t *name;
	wchar_t *value;
};

struct xml_element {
        enum xml_type           type;
	int			is_closed;
	const  wchar_t		*name;
	const  wchar_t		*value;
	struct array		*attr;
	struct xml_element	*next;
	struct xml_element	*prev;
	struct xml_element	*parent;
	struct xml_element	*child;
};

enum xml_state {
	XML_STATE_OPEN,
	XML_STATE_NAME,
        XML_STATE_COMMENT,
	XML_STATE_ATTR,
	XML_STATE_VALUE,
	XML_STATE_CLOSE,
	XML_STATE_END,
	XML_STATE_DISPATCH,
};

struct xml_state_content {
	int		   have_err;
	struct xml_element *tree;
	struct xml_element *curr;
	struct xml_element *tmp;
	const wchar_t *data_curr;
	const wchar_t *data_end;
	enum xml_state last_state;
	enum xml_state curr_state;
};

static struct xml_element *new_elem(enum xml_type type)
{
	struct xml_element *elem;

	elem = (struct xml_element *)malloc(sizeof(*elem));
	
	if (elem) {
		memset(elem, 0, sizeof(*elem));
                elem->type = type;
        }

	return elem;
}

static void xml_free_element(struct xml_element *elm)
{
	assert(elm);

        if (elm->attr)
	        array_release(elm->attr);
	if (elm->name)
		free((wchar_t *)elm->name);
	if (elm->value)
		free((wchar_t *)elm->value);

	free(elm);
}

static int add_brother(struct xml_element **dst, struct xml_element *src)
{
	struct xml_element *elm;

	if (*dst == NULL) {
		*dst = src;
		return 0;
	}

	for (elm = *dst ; elm->next; elm = elm->next)
		;

	elm->next = src;
	src->prev = elm;
        src->parent = elm->parent;       

	return 0;
}

static int add_child(struct xml_element **dst, struct xml_element *src)
{
	struct xml_element *elm;

	if (*dst == NULL) {
		*dst = src;
		return 0;
	}

	elm = *dst;
	if (elm->child == NULL) {
		elm->child = src;
		src->parent = elm;
		return 0;
	}

	add_brother(&elm->child, src);

	return 0;
}

static int close_elem(struct xml_state_content *content)
{
        int len;
	assert(content);

	if (content->tmp) {
                len = strlen_t(content->data_curr, content->data_end, L">");
		content->tmp->is_closed = 1;
                content->data_curr += len + 1;
        }

	return 0;
}

static int add_elem(struct xml_state_content *content)
{
	wchar_t *tag;
	int len;
	struct xml_element *src;

	src = content->tmp;

	if (src == NULL) {
		if (content->data_end - content->data_curr < 2 || *content->data_curr != L'<' && *(content->data_curr + 1) != L'/') {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		content->data_curr += 2;

		len = strlen_t(content->data_curr, content->data_end, L">");
		if (len == 0) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		tag = (wchar_t *)alloca((len + 1) * sizeof(wchar_t));
		if (tag == NULL) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		strcpy_t(tag, content->data_curr, L">");

		assert(content->curr);
		if (wcscmp(tag, content->curr->name) != 0) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		content->curr->is_closed = 1;
		if (content->curr->parent)
			content->curr = content->curr->parent;
	
		content->data_curr += len + 1;

		return 0;
	}


	if (content->curr == NULL || content->tree == NULL) {
		content->curr = src;
		content->tree = src;

		return 0;
	}

        if (content->curr->type == XML_ROOT) {
                add_child(&content->curr, src);
        } else if (content->curr->is_closed) {
		add_brother(&content->curr, src);
        } else {
		add_child(&content->curr, src);
        }

	content->curr = content->tmp;

	if (content->curr->is_closed && content->curr->parent)
		content->curr = content->curr->parent;

	content->tmp = NULL;

	return 0;
}


static int state_open(struct xml_state_content *content)
{
	const wchar_t *data;
	content->data_curr = skip_space(content->data_curr, content->data_end);
	data = content->data_curr;

	if (data >= content->data_end) {
		content->have_err = 1;
		content->curr_state = XML_STATE_END;

		return 0;
	}
	
	while (*data != L'<' && data < content->data_end)
		data++;

        if (data >= content->data_end) {
                content->curr_state = XML_STATE_END;
                return 0;
        }
	if (*data == L'<')
		data++;

	if (*data == L'?') {
	        content->tmp = new_elem(XML_ROOT);
		data++;
        } else if (*data == L'!' && *(data + 1) == L'-' && *(data + 2) == L'-') {
                content->tmp = new_elem(XML_COMMENT);
                data += 3;
        } else {
                content->tmp = new_elem(XML_ELEMENT);
        }

	content->data_curr = data;


	content->curr_state = XML_STATE_DISPATCH;

	return 0;
}

static int state_name(struct xml_state_content *content)
{
	int name_len;
	wchar_t *name;

	name_len = strlen_t(content->data_curr, content->data_end, L">"XML_SPACE_STR);
	
	name = (wchar_t *)malloc(name_len * sizeof(wchar_t) + sizeof(wchar_t));
	if (name == NULL) {
		content->have_err = 1;
		content->curr_state = XML_STATE_END;
		return 0;
	}

	strcpy_t(name, content->data_curr, L">"XML_SPACE_STR);

	content->tmp->name = name;

	if (name[name_len - 1] == L'/' || name[name_len - 1] == L'?') {
		name[name_len - 1] = 0;
		content->curr_state = XML_STATE_OPEN;

                if (content->tmp->type == XML_ELEMENT)
                        content->tmp->type = XML_ELEMENT_SELF;

		close_elem(content);
		add_elem(content);
	} else {
		content->curr_state = XML_STATE_DISPATCH;
	        content->data_curr += name_len;
	}


	return 0;

}

static int state_comment(struct xml_state_content *content)
{
 	int name_len;
	wchar_t *name;

	name_len = strlen_t(content->data_curr, content->data_end, L"-");
	name = (wchar_t *)malloc(name_len * sizeof(wchar_t) + sizeof(wchar_t));
	if (name == NULL) {
		content->have_err = 1;
		content->curr_state = XML_STATE_END;
		return 0;
	}

	strcpy_t(name, content->data_curr, L"-");

	content->tmp->name = name;
	close_elem(content);
        add_elem(content);

        content->curr_state = XML_STATE_DISPATCH;

	return 0;

       
}

static int state_attr(struct xml_state_content *content)
{
	int len, len2;
	int attr_cnt;
	struct xml_attr	attr;
	attr_cnt = str_count(content->data_curr, content->data_end, L'=', L'>', 0);
	if (attr_cnt == 0) {
		content->have_err = 1;
		content->curr_state = XML_STATE_END;
		return 0;
	}
	
	assert(content->tmp);
	content->tmp->attr = array_create(sizeof(struct xml_attr));
	if (content->tmp->attr == NULL) {
		content->have_err = 1;
		content->curr_state = XML_STATE_END;
		return 0;
	}

	array_reserve(content->tmp->attr, attr_cnt);

	while (attr_cnt--) {
		content->data_curr = skip_space(content->data_curr, content->data_end);
		if (content->data_curr >= content->data_end) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		len = strlen_t(content->data_curr, content->data_end, L"=");
		len2 = strlen_t(content->data_curr + len + 2, content->data_end, L"\">");

		if (*(content->data_curr + len) != L'=' ||
			*(content->data_curr + len +1) != L'\"' ||
			(*(content->data_curr + len + len2 + 2)!= L'\"' && *(content->data_curr + len + len2 + 2) != L'?')
			) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		if (content->data_curr + len + len2 >= content->data_end) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
		}

		attr.name = (wchar_t *)malloc(len *sizeof(wchar_t) + sizeof(wchar_t));
		if (attr.name == NULL) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}

		strcpy_t(attr.name, content->data_curr, L"="XML_SPACE_STR);

		content->data_curr += len;
		content->data_curr += 2;

		attr.value = (wchar_t *)malloc(len2 * sizeof(wchar_t) + sizeof(wchar_t));
		if (attr.value == NULL) {
			free(attr.name);
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}
		strcpy_t(attr.value, content->data_curr, L"\">");
		content->data_curr += len2 + 1;

		if (array_push(content->tmp->attr, &attr)) {
			free(attr.name);
			free(attr.value);
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
			return 0;
		}
	}

	content->curr_state = XML_STATE_DISPATCH;

	return 0;
}
static int state_value(struct xml_state_content *content)
{
	int len;
	wchar_t *value;
	content->data_curr = skip_space(content->data_curr, content->data_end);

        if (content->data_end - content->data_curr >= 2 &&
                *content->data_curr == L'<' &&
                *(content->data_curr + 1) == L'/') {
                content->curr_state = XML_STATE_DISPATCH;
                return 0;
        } else if (*content->data_curr == L'<') {
		add_elem(content);
		content->curr_state = XML_STATE_OPEN;
		return 0;
	}

	len = strlen_t(content->data_curr, content->data_end, L"<");

	assert(content->tmp);

	value = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
	if (value == NULL) {
		content->curr_state = XML_STATE_END;
		return 0;
	}

	strcpy_t(value, content->data_curr, L"<");

	content->tmp->value = value;
	content->data_curr += len;

	
	content->curr_state = XML_STATE_DISPATCH;

	return 0;
}
static int state_close(struct xml_state_content *content)
{

	close_elem(content);
	add_elem(content);
	
	content->curr_state = XML_STATE_DISPATCH;

	return 0;
}
static int state_end(struct xml_state_content *content)
{
        if (content->have_err == 0)
                return 0;
 
        if (content->tmp)
                xml_free_element(content->tmp);
        if (content->tree)
                xml_free(content->tree);

        content->tree = NULL;

	return 0;
}
static int state_dispatch(struct xml_state_content *content)
{
	if (content->last_state != XML_STATE_CLOSE && content->data_curr >= content->data_end) {
		content->have_err = 1;
		content->curr_state = XML_STATE_END;
		return 0;
	}

	switch (content->last_state) {
        case XML_STATE_OPEN:
                if (str_issapce(*content->data_curr) ||
                        (content->data_curr + 2 >= content->data_end)) {
                        content->have_err = 1;
                        content->curr_state = XML_STATE_END;
                }
                
                if (content->tmp == NULL) {
                        content->have_err = 1;
                        content->curr_state = XML_STATE_END;

                        return 0;
                }

                if (content->tmp->type == XML_COMMENT)
                        content->curr_state = XML_STATE_COMMENT;
                else
                        content->curr_state = XML_STATE_NAME;
                break;
	case XML_STATE_NAME:
		if (str_issapce(*content->data_curr)) {
			content->curr_state = XML_STATE_ATTR;
		} else if (content->data_end - content->data_curr > 1 && *content->data_curr == L'>') {
			content->data_curr += 1;
			content->curr_state = XML_STATE_VALUE;
		} else {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
		}
		break;
        case XML_STATE_COMMENT:
		content->data_curr = skip_space(content->data_curr, content->data_end);
                if (content->data_curr == content->data_end)
                        content->curr_state = XML_STATE_END;

                if (content->data_end - content->data_curr > 2 && *content->data_curr == L'<' && *(content->data_curr + 1) == L'/')
			content->curr_state = XML_STATE_CLOSE;
                else
                        content->curr_state = XML_STATE_OPEN;
                break;
	case XML_STATE_ATTR:
		content->data_curr = skip_space(content->data_curr, content->data_end);
		if (content->data_curr >= content->data_end) {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
		} else if (*content->data_curr == L'>') {
			content->data_curr += 1;
			content->curr_state = XML_STATE_VALUE;
		} else if ((content->data_end - content->data_curr > 2 && *content->data_curr == L'/' && *(content->data_curr + 1) == L'>') ||
			(content->data_end - content->data_curr > 2 && *content->data_curr == L'?' && *(content->data_curr + 1) == L'>')) {
			content->curr_state = XML_STATE_CLOSE;
			content->data_curr += 1;
		} else {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
		}
		break;
	case XML_STATE_VALUE:
		content->data_curr = skip_space(content->data_curr, content->data_end);
		if (*content->data_curr == L'<' && *(content->data_curr + 1) == L'/') {
			content->curr_state = XML_STATE_CLOSE;
		} else {
			content->have_err = 1;
			content->curr_state = XML_STATE_END;
		}
		break;
	case XML_STATE_CLOSE:
		content->data_curr = skip_space(content->data_curr, content->data_end);
		if (content->data_end - content->data_curr > 2 && *content->data_curr == L'<' && *(content->data_curr + 1) == L'/')
			content->curr_state = XML_STATE_CLOSE;
		else if (content->data_curr < content->data_end)
			content->curr_state = XML_STATE_OPEN;
		else
			content->curr_state = XML_STATE_END;
		break;

	}
	return 0;
}

typedef int (state_func_t)(struct xml_state_content *content);
state_func_t *state_func_tbl[] = {
	state_open,
	state_name,
        state_comment,
	state_attr,
	state_value,
	state_close,
	state_end,
	state_dispatch,
};

static xml_element *parse_data(const wchar_t *data, unsigned long size)
{
	enum xml_state last_state;
	struct xml_state_content state_content;

	assert(size % 2 == 0);
	assert(data);

	if (size < 2)
		return NULL;

	memset(&state_content, 0, sizeof(state_content));

	state_content.data_curr = data;
	state_content.data_end = data + size / sizeof(wchar_t);

	if (*state_content.data_curr == 0xfeff)
		state_content.data_curr += 1;



	state_content.tree = NULL;
	last_state = state_content.last_state = state_content.curr_state = XML_STATE_OPEN;

	while (state_content.last_state != XML_STATE_END) {
		last_state = state_content.curr_state;
		state_func_tbl[state_content.curr_state](&state_content);
		state_content.last_state = last_state;
	}

        assert(state_content.have_err == 0);

	return state_content.tree;
}

struct xml_element *xml_load_file(const wchar_t *path)
{
	FILE *fp;
	wchar_t	*data;
	struct _stat	st;
	struct xml_element	*tree;

	if (_wstat(path, &st) == -1)
		return NULL;
	
	tree = NULL;
	fp = NULL;
	data = NULL;

	fp = _wfopen(path, L"rb+");
	if (fp == NULL)
		goto end;

	data = (wchar_t *)malloc(st.st_size);
	if (data == NULL)
		goto end;
	
	if (fread(data, 1, st.st_size, fp) != (size_t)st.st_size)
		goto end;
 
	tree = parse_data(data, st.st_size);

end:
	if (fp)
		fclose(fp);
	if (data)
		free(data);

	return tree;
}



int xml_free_child(struct xml_element *tree)
{
	struct xml_element *tmp;

        assert(tree);
	if (tree == NULL)
		return 0;
        
        tree = tree->child;
        if (tree == NULL)
                return 0;

        tree->parent->child = NULL;

        while (tree) {
                tmp = tree;
		if (tmp->child)
			xml_free(tmp->child);
		
                tree = tree->next;
		xml_free_element(tmp);
	}
        
        return 0;
}
int xml_free(struct xml_element *tree)
{
	if (tree == NULL)
		return 0;
 
        if (tree->parent && tree->parent->child == tree) {
                assert(tree->prev == NULL);
                tree->parent->child = tree->next;
        } else if (tree->prev) {
                tree->prev->next = tree->next;
        } else {
                //do nothing
        }

        xml_free_child(tree);

        xml_free_element(tree);

        return 0;
}

enum xml_type xml_get_type(const struct xml_element *node)
{
        assert(node);

        return node->type;
}

const wchar_t *xml_get_attr(const struct xml_element *node, const wchar_t *attr_name)
{
        int i;

        assert(node);
        assert(attr_name);

        for (i = 0; i < array_size(node->attr); i++) {
                if (wcscmp(array_at(node->attr, i, struct xml_attr).name, attr_name) == 0)
                        break;
        }
        
        if (i >= array_size(node->attr))
                return NULL;

        return array_at(node->attr, i, struct xml_attr).value; 
}

const wchar_t *xml_get_name(const struct xml_element *node)
{
        assert(node);
        return node->name;
}

const wchar_t *xml_get_value(const struct xml_element *node)
{
        assert(node);
        return node->value;
}
/* TODO:
 *      定义一种字符串类型记录内存分配大小与字串长度
 *      如果当前字符串比较短直接copy即可
 */


wchar_t *xml_set_value(struct xml_element *node, const wchar_t *value)
{
        int len;
        wchar_t *v;

        assert(value);

        v = (wchar_t *)node->value;

        if (node->value) {
                free(v);
                node->value = NULL;
        }
        
        len = wcslen(value);
        v = (wchar_t *) malloc((len + 1)* sizeof(wchar_t));
        if (v == NULL)
                return NULL;

        wcsncpy(v, value, len);
        v[len] = 0;
 
        node->value =v;

        return (wchar_t *)value;
}


struct xml_element *xml_walkdown(const struct xml_element *node)
{
        assert(node);
        return node->child;
}

struct xml_element *xml_walkup(const struct xml_element *node)
{
        assert(node);
        return node->parent;
}

struct xml_element *xml_walknext(const struct xml_element *node)
{
        assert(node);
        return node->next;
}

struct xml_element *xml_walkprev(const struct xml_element *node)
{
        assert(node);
        return node->next;
}
struct xml_element *xml_search_child(const struct xml_element *parent, const wchar_t *name)
{
        struct xml_element *elm;

        assert(parent);

        for (elm = parent->child; elm; elm = elm->next) {
               if (wcscmp(elm->name, name) == 0)
                       break;
        }

        return elm;
}

struct xml_element *xml_search_brother(struct xml_element *brother, const wchar_t *name)
{
        struct xml_element *elm;

        assert(brother);

        for (elm = brother; elm; elm = elm->next) {
               if (wcscmp(elm->name, name) == 0)
                       break;
        }

        return elm;
}


struct xml_element *xml_new(const wchar_t *name, const wchar_t *value, enum xml_type type)
{
        int name_len;
        int value_len;
        wchar_t *name_tmp;
        wchar_t *value_tmp;
        struct xml_element      *elm;

        assert(name);
 
        elm = (struct xml_element *)malloc(sizeof(*elm));
        if (elm == NULL)
                return elm;

        memset(elm, 0, sizeof(*elm));
        elm->is_closed = 1;
        elm->type = type;

        name_len = wcslen(name);
        if (value)
                value_len = wcslen(value);
        else
                value_len = 0;
 
        if (name_len == 0) {
                free(elm);
                return NULL;
        }

        if (name_len) {
                name_tmp = (wchar_t *)malloc((name_len + 1) * sizeof(wchar_t));
                wcsncpy(name_tmp, name, name_len);
                name_tmp[name_len] = 0;
        } else {
                name_tmp = NULL;
        }

        if (value_len) {
                value_tmp = (wchar_t *)malloc((value_len + 1) * sizeof(wchar_t));
                wcsncpy(value_tmp, value, value_len);
                value_tmp[value_len] = 0;
        } else {
                value_tmp = NULL;
        }

        if (name_tmp == NULL) {
                if (value_tmp)
                        free(value_tmp);
                free(elm);

                return NULL;
        }

        elm->name = name_tmp;
        elm->value = value_tmp;

        return elm;
}

struct xml_element *xml_append_child(struct xml_element *parent, struct xml_element *child)
{
        struct xml_element *tmp;
        assert(parent);
        assert(child);

	assert(parent->value == NULL);

        if (parent->child == NULL) {
                parent->child = child;
                child->parent = parent;
                if (parent->type == XML_ELEMENT_SELF)
                        parent->type = XML_ELEMENT;
                return child;
        }

        for (tmp = parent->child; tmp->next; tmp = tmp->next)
                ;

        tmp->next = child;
        child->prev = tmp;
        child->parent = tmp->parent;

        return child;
}

struct xml_element *xml_append_brother(struct xml_element *b1, struct xml_element *b2)
{
        struct xml_element *tmp;

        assert(b1);
        assert(b2);

        assert(b1->is_closed);

        for (tmp = b1; tmp->next; tmp = tmp->next)
                ;

        tmp->next = b2;
        b2->prev = tmp;
        b2->parent = b1->parent;

        return b2;
}

static int format_name(const struct xml_element *elm, wchar_t *buff, int size, int descent)
{
        int i;
        int len;

        assert(elm);
        assert(buff);

        len = 0;
 
        assert(size > descent);
        if (size < descent)
                return 0;

        len += descent;
        while (descent--)
                buff[descent] = L'\t';
        if (elm->type == XML_ROOT)
                len += swprintf(buff + len, L"<?%s", elm->name);
        else if (elm->type == XML_COMMENT)
                len += swprintf(buff + len, L"<!--%s", elm->name);
        else if (elm->type == XML_ELEMENT || elm->type == XML_ELEMENT_SELF)
                len += swprintf(buff + len, L"<%s", elm->name);
        else
                assert(!"unknow xml element type");

        for (i = 0; i < array_size(elm->attr); i++) {
                len += swprintf(buff + len, L"\t%s=\"%s\"\r\n",
                                array_at(elm->attr, i, struct xml_attr).name,
                                array_at(elm->attr, i, struct xml_attr).value
                        );
        }
        
        if (elm->type == XML_ELEMENT_SELF) {
                assert(elm->value == NULL);
                len += swprintf(buff + len, L"/>");
        } else if (elm->value && elm->type == XML_ELEMENT) {
                len += swprintf(buff + len, L">%s", elm->value);
        } else if (elm->child && elm->type == XML_ELEMENT) {
                len += swprintf(buff + len, L">\r\n");
        } else if (elm->type == XML_ELEMENT) {
                len += swprintf(buff + len, L">");
        } else if (elm->type == XML_COMMENT) {
                len += swprintf(buff + len, L"-->\r\n");
        } else if (elm->type == XML_ROOT) {
                len += swprintf(buff + len, L"?>\r\n");
        } else {
                assert(!"oh, i forget this condition");
        }
        return len;
}

static int format_end(const struct xml_element *elm, wchar_t *buff, int size, int descent)
{
        int len;
        assert(elm);
        assert(buff);
 
        assert(size >= descent);
        if (size < descent)
                return 0;

        len = 0;
        
        if (elm->type == XML_ELEMENT && elm->child) {
                len += descent;
                while (descent--)
                        buff[descent] = L'\t';
        }

        if (elm->type == XML_ELEMENT)
                len += swprintf(buff + len, L"</%s>\r\n", elm->name);
        else if (elm->type == XML_COMMENT)
                len += swprintf(buff + len, L"-->");

        return len;
}

static int cacl_name(const struct xml_element *elm, int descent)
{
        int i;
        int len;

        assert(elm);

        len = 0;
 
        len += descent;
        if (elm->type == XML_ROOT) {
                len += 2;       //swprintf(buff + len, L"<?%s", elm->name);
                len += wcslen(elm->name);
        } else if (elm->type == XML_COMMENT) {
                len += 4;
                len += wcslen(elm->name);       //L"<!--%s", elm->name);
        } else if (elm->type == XML_ELEMENT || elm->type == XML_ELEMENT_SELF) {
                len += 1; //L"<%s"
                len += wcslen(elm->name);
        } else {
                assert(!"unknow xml element type");
        }

        for (i = 0; i < array_size(elm->attr); i++) {
                len += 6;       //L"\t%s=\"%s\"\r\n"
                len += wcslen(array_at(elm->attr, i, struct xml_attr).name);
                len += wcslen(array_at(elm->attr, i, struct xml_attr).value);
        }
        
        if (elm->type == XML_ELEMENT_SELF) {
                assert(elm->value == NULL);
                len += 2;       //L"/>"
        } else if (elm->value && elm->type == XML_ELEMENT) {
                len += 1;       //L">%s
                len += wcslen(elm->value);
        } else if (elm->child && elm->type == XML_ELEMENT) {
                len += 3;       //L">\r\n"
        } else if (elm->type == XML_ELEMENT) {
                len += 1;
                //len += swprintf(buff + len, L">");
        } else if (elm->type == XML_COMMENT) {
                len += 5;
                //len += swprintf(buff + len, L"-->\r\n");
        } else if (elm->type == XML_ROOT) {
                len += 4;
                //len += swprintf(buff + len, L"?>\r\n");
        } else {
                assert(!"oh, i forget this condition");
        }

        return len;
}

static int cacl_end(const struct xml_element *elm, int descent)
{
        int len;
        assert(elm);

        len = 0;

        if (elm->type == XML_COMMENT || elm->type == XML_ROOT)
                return 0;

        if (elm->type == XML_ELEMENT && elm->child) {
                len += descent;
        }

        if (elm->type == XML_ELEMENT) {
                len += 5;       //L"</%s>\r\n",
                len += wcslen(elm->name);
        } else if (elm->type == XML_COMMENT) {
                len += 3;       //L"-->"
        }

        return len;
}

static int format_tree(const struct xml_element *tree, wchar_t *buff, unsigned long cnt, int descent)
{
        int size;
        const struct xml_element *tmp;

        assert(tree);
        assert(buff);
 
	if (tree == NULL)
		return 0;

        size = 0;

        while (tree) {
                tmp = tree;
                size += format_name(tmp, buff + size, cnt - size, descent);

                descent += 1;

		if (tmp->child)
			size += format_tree(tmp->child, buff + size, cnt - size, descent);

                tree = tree->next;

                descent -= 1;
                assert(descent >= 0);

		size += format_end(tmp, buff + size, cnt - size, descent);
	}

        return size;
}

static int cacl_tree(const struct xml_element *tree, int descent)
{
        int size;
        const struct xml_element *tmp;

        assert(tree);
	if (tree == NULL)
		return 0;

        size = 0;

        while (tree) {
                tmp = tree;
                size += cacl_name(tmp, descent);

                descent += 1;

		if (tmp->child)
			size += cacl_tree(tmp->child, descent);

                tree = tree->next;

                descent -= 1;
                assert(descent >= 0);

		size += cacl_end(tmp, descent);
	}

        return size;
}

int xml_need_len(const struct xml_element *tree)
{
        int size;
        assert(tree);
	if (tree == NULL)
		return 0;

        size = cacl_tree(tree, 0);

        return size + 1;
}

int xml_save_data(const struct xml_element *tree, wchar_t *buff, unsigned long cnt)
{
        int size;
 
        assert(tree);
        assert(buff);
        if (cnt < 2)
                return -1;

        //unicode
        buff[0] = 0xfeff;
        buff++;
        cnt--;

        size = format_tree(tree, buff, cnt, 0);

        return size + 1;
}

