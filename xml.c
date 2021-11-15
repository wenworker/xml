#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "xml.h"

#define BUFFER_SIZE 10*1024
#define STACK_SIZE  10

typedef struct header_t
{
    char* name;
    char* value;
    struct header_t* next;
} header_t;

typedef struct attribute_t
{
    char* name;
    char* value;
    struct attribute_t* next;
} attribute_t;

typedef struct element_t
{
    char* ns;	//namespace
    char* name;
    char* text;
    attribute_t* attributes;
    struct element_t* parent;
    struct element_t* children;
    struct element_t* siblings;	// == next
} element_t, *xml_element_t;

typedef struct gb_xml_t
{
    char    buffer[BUFFER_SIZE];
    int     buffer_used;
    void*   extend[2];
    int     status;

    header_t*   header;
    element_t*  element;
} gb_xml_t, *xml_handle_t;

typedef struct
{
    void*   data[STACK_SIZE];
    int     header;
} stack_t;

static int init_stack(stack_t* stack)
{
    if (stack == NULL) {
        return -1;
    }

    memset(stack->data, 0x0, sizeof(stack->data));
    stack->header = -1;

    return 0;
}

static void* pop_stack(stack_t* stack)
{
    if (stack == NULL || stack->header < 0) {
        return NULL;
    }

    return stack->data[stack->header--];
}

static int push_stack(stack_t* stack, void* item)
{
    if (stack == NULL || stack->header + 1 >= STACK_SIZE || item == NULL) {
        return -1;
    }

    stack->data[++stack->header] = item;
    return 0;
}

static void* read_stack(stack_t* stack)
{
    if (stack == NULL || stack->header < 0) {
        return NULL;
    }

    return stack->data[stack->header];
}

static void* xml_malloc(xml_handle_t xml, size_t size)
{
    if (xml == NULL || size == 0) {
        return NULL;
    }

    if (xml->buffer_used + size > sizeof(xml->buffer)) {
        return NULL;
    }

    void* pointer = xml->buffer + xml->buffer_used;
    xml->buffer_used += size;
    return pointer;
}

static char* xml_newstr(xml_handle_t xml)
{
    if (xml == NULL) {
        return NULL;
    }

    if (xml->buffer_used >= sizeof(xml->buffer)) {
        return NULL;
    }

    return xml->buffer + xml->buffer_used;
}

static char* xml_strinc(xml_handle_t xml, char* dst, char c)
{
    if (xml != NULL && dst != NULL) {
        if (xml->buffer_used + 1 < sizeof(xml->buffer)) {
            xml->buffer[xml->buffer_used++] = c;
            return dst;
        }
    }

    return NULL;
}

static char* xml_strdup(xml_handle_t xml, const char* src)
{
    if (xml == NULL || src == NULL || strlen(src) == 0) {
        return NULL;
    }

    if (xml->buffer_used + strlen(src) > sizeof(xml->buffer)) {
        return NULL;
    }

    char* dst = xml->buffer + xml->buffer_used;
    strncpy(dst, src, strlen(src));
    xml->buffer_used += strlen(src);
    return dst;
}

static char* xml_strdup2(xml_handle_t xml, const char* src)
{
    char* dst = xml_strdup(xml, src);
    if (dst == NULL) {
        return NULL;
    }

    if (xml->buffer_used + 1 > sizeof(xml->buffer)) {
        return NULL;
    }
    xml->buffer[xml->buffer_used++] = '\0';
    return dst;
}

typedef enum
{
    NODE_UNKNOWN = -1,
    NODE_HEADER = 0,
    NODE_OPEN_TAG,
    NODE_CLOSE_TAG,
    NODE_SINGLE_TAG,    // <... />
    NODE_TEXT,
    NODE_BLANK,
} XML_NODE_TYPE;

static int get_node_type(const char* node, int size)
{
    if (node == NULL || size <= 0) {
        return NODE_UNKNOWN;
    }
    if (node[0] == '<' && node[1] == '>') {
        return NODE_UNKNOWN;
    }

    //printf("\033[0;32;31m node=[%s] %s,%d \033[m\n", node, __FILE__, __LINE__);

    if (node[0] == '<') {
        if (node[1] == '/') {
            if (node[size-1] == '>') {
                return NODE_CLOSE_TAG;
            } else {
                return NODE_TEXT;
            }
        } else if (node[1] == '?' && node[size-2] == '?' && node[size-1] == '>') {
            return NODE_HEADER;
        } else if (node[size-1] == '>') {
            if (node[size-2] == '/') {    // <... />
                return NODE_SINGLE_TAG;
            }
            return NODE_OPEN_TAG;
        } else {
            return NODE_TEXT;
        }
    } else {
        int i = 0;
        for (i=0; i<size; i++) {
            if (isspace(node[i]) == 0) {
                return NODE_TEXT;
            }
        }
        return NODE_BLANK;
    }
}

static int add_child(element_t* parent, element_t* child)
{
    if (parent == NULL || child == NULL) {
        return -1;
    }
    element_t** temp = &parent->children;
    while (*temp) {
        if (*temp == child) {   // 防止重复添加
            return 0;
        }
        temp = &(*temp)->siblings;
    }
    *temp = child;

    return 0;
}

static int parse_header(xml_handle_t xml)
{
    if (xml->header != NULL) {
        return xml->status = XML_STATUS_SYNTAX;
    }

    char* node = xml->extend[0];
    if (node == NULL) {
        return xml->status = XML_STATUS_FAULT;
    }
    int size = strlen(node);

    header_t* header = NULL;
    int i = 0;
    for (i=0; i<size; i++) {
        const char c = node[i];
        if (c == ' ') {
            if (header == NULL) {
                header_t** pheader = &xml->header;
                while (*pheader) {
                    pheader = &(*pheader)->next;
                }
                *pheader = xml_malloc(xml, sizeof(header_t));
                if (*pheader == NULL) {
                    return xml->status = XML_STATUS_NO_MEMORY;
                }
                header = *pheader;
            } else {
                header->next = xml_malloc(xml, sizeof(header_t));
                if (header->next == NULL) {
                    return xml->status = XML_STATUS_NO_MEMORY;
                }
                header = header->next;
            }
            header->name = node + i + 1;  // +1 skip ' '
        } else if (c == '=') {
            node[i] = '\0';
            header->value = node + i + 2; // +2 skip '=' and '"'
        } else if (c == '"') {
            node[i] = '\0';
            // do nothing
        } else {

        }
    }

    return xml->status = XML_STATUS_SUCCEED;
}

static int parse_name(xml_handle_t xml, element_t* element)
{
    if (xml == NULL || element == NULL) {
        return XML_STATUS_FAULT;
    }

    char* node = xml->extend[0];
    int size = strlen(node);
    if (node == NULL || size == 0) {
        return XML_STATUS_FAULT;
    }
    element->name = node + 1;   // +1 skip '<'
    attribute_t* attribute = NULL;
    int i = 0;
    for (i=0; i<size; i++) {
        if ( (node[i] == '/' && node[i+1] == '>') || node[i] == '>' ) {
            node[i] = '\0';
            break;
        } else if (node[i] == ' ') {
            node[i] = '\0';
            if (node[i+1] != ' ' && node[i+1] != '/' && node[i+1] != '>') {
                if (attribute == NULL) {
                    attribute_t** pattribute = &element->attributes;
                    while (*pattribute) {
                        pattribute = &(*pattribute)->next;
                    }
                    *pattribute = xml_malloc(xml, sizeof(attribute_t));
                    if (*pattribute == NULL) {
                        return xml->status = XML_STATUS_NO_MEMORY;
                    }
                    attribute = *pattribute;
                } else {
                    attribute->next = xml_malloc(xml, sizeof(attribute_t));
                    if (attribute->next == NULL) {
                        return xml->status = XML_STATUS_NO_MEMORY;
                    }
                    attribute = attribute->next;
                }
                attribute->name = node + i + 1;  // +1 skip ' '
            }
        } else if (node[i] == '=') {
            node[i] = '\0';
            attribute->value = node + i + 2; // +2 skip '=' and '"'
        } else if (node[i] == '"') {
            node[i] = '\0';
            // do nothing
        } else {
            // do nothing
        }
    }

    char* name = element->name;
    int name_size = strlen(name);
    for (i=0; i<name_size; i++) {
        if (name[i] == ':') {
            name[i] = '\0';
            element->ns = name;
            element->name = name + i + 1;   // +1 skip ':'
        }
    }

    return xml->status = XML_STATUS_SUCCEED;
}

static int parse_node(xml_handle_t xml)
{
    if (xml == NULL) {
        return xml->status = XML_STATUS_FAULT;
    }

    char* node = xml->extend[0];
    stack_t* stack = xml->extend[1];
    if (stack == NULL) {
        stack = xml_malloc(xml, sizeof(stack_t));
        if (stack == NULL) {
            return xml->status = XML_STATUS_NO_MEMORY;
        }
        init_stack(stack);
        xml->extend[1] = stack;
    }

    int size = strlen(node);
    int type = get_node_type(node, size);

    if (type == NODE_HEADER) {
        parse_header(xml);
    } else if (type == NODE_OPEN_TAG || type == NODE_SINGLE_TAG) {
        element_t* element = xml_malloc(xml, sizeof(element_t));
        if (element == NULL) {
            return xml->status = XML_STATUS_NO_MEMORY;
        }
        memset(element, 0x0, sizeof(element_t));
        element_t* parent = read_stack(stack);
        if (parent) {
            add_child(parent, element);
        } else {
            xml->element = element;
        }
        if (type == NODE_OPEN_TAG) {
            if (push_stack(stack, element) != 0) {
                return xml->status = XML_STATUS_SYNTAX;
            }
        }
        parse_name(xml, element);
    } else if (type == NODE_CLOSE_TAG) {
        element_t* element = read_stack(stack);
        if (element == NULL || element->name == NULL || strlen(element->name) == 0) {
            return xml->status = XML_STATUS_SYNTAX;
        }
        char temp[128] = {0};
        if (element->ns) {
            snprintf(temp, sizeof(temp), "</%s:%s>", element->ns, element->name);
        } else {
            snprintf(temp, sizeof(temp), "</%s>", element->name);
        }
        if (strcmp(node, temp) != 0) {
            printf("\033[0;32;31m%s != %s\033[m\n", temp, node);
            return XML_STATUS_SYNTAX;
        }
        pop_stack(stack);
    } else if (type == NODE_TEXT) {
        element_t* element = read_stack(stack);
        if (element == NULL) {
            return xml->status = XML_STATUS_SYNTAX;
        }
        element->text = node;
    } else if (type == NODE_BLANK) {
        // discard
        return xml->status = XML_STATUS_SUCCEED;
    } else {
        return xml->status = XML_STATUS_SYNTAX;
    }
    
    return xml->status = XML_STATUS_SUCCEED;
}

static element_t* get_element(element_t* root, const char* ns, const char* name)
{
    if (root == NULL) {
        return NULL;
    }

    if (root->name && strcmp(name, root->name) == 0) {
        if (ns && strlen(ns)) {
            if (root->ns && strcmp(ns, root->ns) == 0) {
                return root;
            }
        } else {
            return root;
        }
    }

    element_t* child = NULL;
    if ( (child = get_element(root->children, ns, name)) != NULL ) {
        return child;
    }

    return get_element(root->siblings, ns, name);
}

static element_t* get_child(element_t* parent, const char* child_ns, const char* child_name)
{
    element_t* child = parent->children;
    while (child) {
        if (strcmp(child->name, child_name) == 0) {
            if (child_ns && strlen(child_ns)) {
                if (child->ns && strcmp(child->ns, child_ns) == 0) {
                    return child;
                }
            } else {
                return child;
            }
        }

        child = child->siblings;
    }

    return NULL;
}

static attribute_t* get_attribute(element_t* element, const char* attribute_name)
{
    if (element == NULL || attribute_name == NULL || strlen(attribute_name) == 0) {
        return NULL;
    }

    attribute_t* attributes = element->attributes;
    while (attributes) {
        if (attributes->name && strcmp(attributes->name, attribute_name) == 0) {
            return attributes;
        }
        attributes = attributes->next;
    }

    return NULL;
}

static element_t* add_element(xml_handle_t xml, element_t* parent, const char* ns, const char* name, const char* text)
{
    if (name == NULL || strlen(name) == 0) {
        return NULL;
    }

    element_t* element = xml_malloc(xml, sizeof(element_t));
    if (element == NULL) {
        return NULL;
    }
    if (xml->element == NULL) {
        xml->element = element;
    }

    if (ns && strlen(ns)) {
        element->ns = xml_strdup2(xml, ns);
    }
    element->name = xml_strdup2(xml, name);
    if (text && strlen(text)) {
        element->text = xml_strdup2(xml, text);
    }
    
    if (parent) {
        element_t** children = &parent->children;
        while (*children) {
            children = &(*children)->siblings;
        }
        *children = element;
    }

    return element;
}

static attribute_t* add_attribute(xml_handle_t xml, element_t* element, const char* name, const char* value)
{
    if (element == NULL || name == NULL || strlen(name)==0 || value == NULL || strlen(value)==0) {
        return NULL;
    }

    attribute_t** attribute = &element->attributes;
    while (*attribute) {
        attribute = &(*attribute)->next;
    }
    *attribute = xml_malloc(xml, sizeof(attribute_t));
    (*attribute)->name = xml_strdup2(xml, name);
    (*attribute)->value = xml_strdup2(xml, value);

    return *attribute;
}

static int serialize_header(xml_handle_t xml, header_t* header)
{
    if (xml == NULL || header == NULL) {
        return xml->status = XML_STATUS_FAULT;
    }
    xml_strdup(xml, "<?xml");
    header_t* temp_header = header;
    while (temp_header) {
        xml_strdup(xml, " ");
        xml_strdup(xml, temp_header->name);
        xml_strdup(xml, "=");
        xml_strdup(xml, "\"");
        xml_strdup(xml, temp_header->value);
        xml_strdup(xml, "\"");
        temp_header = temp_header->next;
    }
    xml_strdup(xml, "?>");

    return xml->status = XML_STATUS_SUCCEED;
}

static int serialize_element(xml_handle_t xml, element_t* element)
{
    if (element == NULL) {
        return xml->status = XML_STATUS_SUCCEED;
    }

    if (xml == NULL) {
        return xml->status = XML_STATUS_FAULT;
    }

    xml_strdup(xml, "<");
    if (element->ns) {
        xml_strdup(xml, element->ns);
        xml_strdup(xml, ":");
    }
    xml_strdup(xml, element->name);
    attribute_t* attributes = element->attributes;
    while (attributes) {
        xml_strdup(xml, " ");
        xml_strdup(xml, attributes->name);
        xml_strdup(xml, "=");
        xml_strdup(xml, "\"");
        xml_strdup(xml, attributes->value);
        xml_strdup(xml, "\"");
        attributes = attributes->next;
    }
    xml_strdup(xml, ">");

    if (element->text) {
        xml_strdup(xml, element->text);
    }

    xml->status = serialize_element(xml, element->children);
    if (xml->status != XML_STATUS_SUCCEED){
        return xml->status;
    }

    xml_strdup(xml, "</");
    if (element->ns) {
        xml_strdup(xml, element->ns);
        xml_strdup(xml, ":");
    }
    xml_strdup(xml, element->name);
    xml_strdup(xml, ">");

    xml->status = serialize_element(xml, element->siblings);
    if (xml->status != XML_STATUS_SUCCEED) {
        return xml->status;
    }

    return xml->status = XML_STATUS_SUCCEED;
}

static void print_header(const header_t* header)
{
    if (header == NULL) {
        return;
    }

    const header_t* tmp = header;
    while (tmp) {
        if (tmp->name) {
            printf("hader_name:%s\n", tmp->name);
        }
        if (tmp->value) {
            printf("header_value:%s\n", tmp->value);
        }
        tmp = tmp->next;
    }

    return;
}

static void print_element(const element_t* element)
{
    if (element){
        printf("\n");
        if (element->ns)
            printf("ns:\t\t[%s]\n", element->ns);
        if (element->name)
            printf("name:\t\t[%s]\n", element->name);
        if (element->text)
            printf("text:\t\t[%s]\n", element->text);
        attribute_t* attribute = element->attributes;
        while (attribute)
        {
            if (attribute->name)
                printf("attribute_name:\t[%s]\n", attribute->name);
            if (attribute->value)
                printf("attribute_value:[%s]\n", attribute->value);
            attribute = attribute->next;
        }
        printf("\n");
    } else{
        return;
    }
    print_element(element->children);
    print_element(element->siblings);

    return;
}

xml_handle_t xml_malloc_handle()
{
    xml_handle_t xml = malloc(sizeof(gb_xml_t));
    if (xml) {
        memset(xml, 0x0, sizeof(gb_xml_t));
    }

    return xml;
}

void xml_free_handle(xml_handle_t xml)
{
    if (xml) {
        free(xml);
    }

    return;
}

int xml_input_raw(xml_handle_t xml, const char* raw, int size)
{
    if (xml == NULL || raw == NULL || strlen(raw) == 0 || size == 0) {
        return xml->status = XML_STATUS_FAULT;
    }

    char* node = xml->extend[0];
    if (node == NULL) {
        xml->extend[0] = node = xml_newstr(xml);
    }

    int i = 0;
    for (i=0; i<size; i++) {
        const char c = raw[i];
        if (c == '<') {
            if (node != NULL && strlen(node) > 0) {
                xml_strinc(xml, node, '\0');
                if (parse_node(xml) == 0) {
                    xml->extend[0] = node = xml_newstr(xml);
                } else {
                    return xml->status = XML_STATUS_SYNTAX;
                }
            }
            xml_strinc(xml, node, c);
        } else if (c == '>') {
            xml_strinc(xml, node, c);
            xml_strinc(xml, node, '\0');
            if (parse_node(xml) == 0) {
                xml->extend[0] = node = xml_newstr(xml);
            } else {
                return xml->status = XML_STATUS_SYNTAX;
            }
        } else if (c == '\r' || c == '\n') {
            continue;
        } else {
            xml_strinc(xml, node, c);
        }
    }

    return xml->status = XML_STATUS_SUCCEED;
}

const char* xml_get_text(xml_handle_t xml, const char* element_ns, const char* element_name)
{
    if (xml == NULL || element_name == NULL || strlen(element_name) == 0){
        return NULL;
    }

    element_t* element = get_element(xml->element, element_ns, element_name);
    if (element) {
        return element->text;
    } else {
        return NULL;
    }
}

int xml_get_int(xml_handle_t xml, const char* element_ns, const char* element_name)
{
    const char* text = xml_get_text(xml, element_ns, element_name);
    if (text) {
        return atoi(text);
    } else {
        return -1;
    }
}

float xml_get_float(xml_handle_t xml, const char* element_ns, const char* element_name)
{
    const char* text = xml_get_text(xml, element_ns, element_name);
    if (text) {
        return atof(text);
    } else {
        return 0.0;
    }
}

const char* xml_get_attribute_text(xml_handle_t xml, const char* element_ns, const char* element_name, const char* attribute_name)
{
    if (xml == NULL || element_name == NULL || strlen(element_name) == 0 || attribute_name == NULL || strlen(attribute_name) == 0) {
        return NULL;
    }

    element_t* element = get_element(xml->element, element_ns, element_name);
    if (element == NULL) {
        return NULL;
    }

    attribute_t* attribute = get_attribute(element, attribute_name);
    if (attribute) {
        return attribute->value;
    }

    return NULL;
}

int xml_get_attribute_int(xml_handle_t xml, const char* element_ns, const char* element_name, const char* attribute_name)
{
    const char* text = xml_get_attribute_text(xml, element_ns, element_name, attribute_name);
    if (text) {
        return atoi(text);
    } else {
        return -1;
    }
}

float xml_get_attribute_float(xml_handle_t xml, const char* element_ns, const char* element_name, const char* attribute_name)
{
    const char* text = xml_get_attribute_text(xml, element_ns, element_name, attribute_name);
    if (text) {
        return atof(text);
    } else {
        return 0.0;
    }
}

xml_element_t xml_get_element(xml_handle_t xml)
{
    if (xml == NULL) {
        return NULL;
    }

    return xml->element;
}

xml_element_t element_get_sibling(xml_element_t element)
{
    if (element == NULL) {
        return NULL;
    }

    return element->siblings;
}

xml_element_t element_get_child(xml_element_t element, const char* child_ns, const char* child_name)
{
    if (element == NULL || child_name == NULL || strlen(child_name) == 0) {
        return NULL;
    }

    return get_child(element, child_ns, child_name);
}

const char* element_get_child_text(xml_element_t element, const char* child_ns, const char* child_name)
{
    if (element == NULL || child_name == NULL || strlen(child_name) == 0) {
        return NULL;
    }

    element_t* child = get_child(element, child_ns, child_name);
    if (child == NULL) {
        return NULL;
    }

    return child->text;
}

int element_get_child_int(xml_element_t element, const char* child_ns, const char* child_name)
{
    const char* text = element_get_child_text(element, child_ns, child_name);
    if (text) {
        return atoi(text);
    } else {
        return -1;
    }
}

float element_get_child_float(xml_element_t element, const char* child_ns, const char* child_name)
{
    const char* text = element_get_child_text(element, child_ns, child_name);
    if (text) {
        return atof(text);
    } else {
        return 0.0;
    }
}

const char* element_get_text(xml_element_t element)
{
    if (element == NULL) {
        return NULL;
    }

    return element->text;
}

int element_get_int(xml_element_t element)
{
    const char* text = element_get_text(element);
    if (text) {
        return atoi(text);
    } else {
        return -1;
    }
}

float element_get_float(xml_element_t element)
{
    const char* text = element_get_text(element);
     if (text) {
        return atof(text);
    } else {
        return 0.0;
    }
}

const char* element_get_attribute_text(xml_element_t element, const char* attribute_name)
{
    if (element == NULL || attribute_name == NULL || strlen(attribute_name) == 0) {
        return NULL;
    }

    attribute_t* attribute = get_attribute(element, attribute_name);
    if (attribute) {
        return attribute->value;
    } else {
        return NULL;
    }
}

int element_get_attribute_int(xml_element_t element, const char* attribute_name)
{
    const char* text = element_get_attribute_text(element, attribute_name);
    if (text) {
        return atoi(text);
    } else {
        return -1;
    }
}

float element_get_attribute_float(xml_element_t element, const char* attribute_name)
{
    const char* text = element_get_attribute_text(element, attribute_name);
    if (text) {
        return atof(text);
    } else {
        return 0.0;
    }
}

int xml_add_element(xml_handle_t xml, const char* parent_ns, const char* parent_name, const char* ns, const char* name, XML_VALUE_TYPE type, const void* value)
{
    if (xml == NULL || name == NULL || strlen(name) == 0) {
        return XML_STATUS_FAULT;
    }

    element_t* parent = NULL;
    if (parent_name && strlen(parent_name) > 0) {
        parent = get_element(xml->element, parent_ns, parent_name);
        if (parent == NULL) {
            return XML_STATUS_FAULT;
        }
    }

    const char* text = NULL;
    char tmp[64] = {0};
    if (value) {
        if (type == XML_VALUE_TYPE_INT) {
            snprintf(tmp, sizeof(tmp), "%d", *((int*)value));
            text = tmp;
        } else if (type == XML_VALUE_TYPE_FLOAT) {
            snprintf(tmp, sizeof(tmp), "%f", *((float*)value));
            text = tmp;
        } else {
            text = value;
        }
    }

    if (add_element(xml, parent, ns, name, text) != NULL) {
        return XML_STATUS_SUCCEED;
    } else {
        return XML_STATUS_FAULT;
    }
}

int xml_add_attribute(xml_handle_t xml, const char* element_ns, const char* element_name, const char* name, XML_VALUE_TYPE type, const void* value)
{
    if (xml == NULL || element_name == NULL || strlen(element_name) == 0 || name == NULL || strlen(name) == 0 || value == NULL || strlen(value) == 0) {
        return XML_STATUS_FAULT;
    }

    element_t* element = get_element(xml->element, element_ns, element_name);
    if (element == NULL) {
        return XML_STATUS_FAULT;
    }

    const char* text = NULL;
    char tmp[64] = {0};
    if (value) {
        if (type == XML_VALUE_TYPE_INT) {
            snprintf(tmp, sizeof(tmp), "%d", *((int*)value));
            text = tmp;
        } else if (type == XML_VALUE_TYPE_FLOAT) {
            snprintf(tmp, sizeof(tmp), "%f", *((float*)value));
            text = tmp;
        } else {
            text = value;
        }
    }

    if (add_attribute(xml, element, name, text) != NULL) {
        return XML_STATUS_SUCCEED;
    } else {
        return XML_STATUS_FAULT;
    }
}

const char* xml_serialize(xml_handle_t xml)
{
    if (xml == NULL) {
        return NULL;
    }

    char* ret = xml->buffer + xml->buffer_used;
    if (xml->header) {
        serialize_header(xml, xml->header);
    } else {
        xml_strdup(xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    }
    
    serialize_element(xml, xml->element);

    return ret;
}

void xml_debug_print(xml_handle_t xml)
{
    if (xml == NULL) {
        return;
    }

    print_header(xml->header);
    print_element(xml->element);

    printf("buffer_used:%d\n", xml->buffer_used);

    return;
}