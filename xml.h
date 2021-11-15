#ifndef __XML_H__
#define __XML_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    XML_STATUS_SUCCEED = 0,
    XML_STATUS_NO_MEMORY,
    XML_STATUS_SYNTAX,
    XML_STATUS_FAULT,
} XML_STATUS;

typedef enum
{
    XML_VALUE_TYPE_TEXT,
    XML_VALUE_TYPE_INT,
    XML_VALUE_TYPE_FLOAT,
} XML_VALUE_TYPE;

// typedef
typedef struct gb_xml_t* xml_handle_t;
typedef struct element_t* xml_element_t;

// xml handle
xml_handle_t xml_malloc_handle();

void xml_free_handle(xml_handle_t handle);

// input raw data
int xml_input_raw(xml_handle_t xml, const char* raw, int size);

// if element name is unique in xml, use below method to get element's value or attribute
const char* xml_get_text(xml_handle_t xml, const char* element_ns, const char* element_name);

int xml_get_int(xml_handle_t xml, const char* element_ns, const char* element_name);

float xml_get_float(xml_handle_t xml, const char* element_ns, const char* element_name);

const char* xml_get_attribute_text(xml_handle_t xml, const char* element_ns, const char* element_name, const char* attribute_name);

int xml_get_attribute_int(xml_handle_t xml, const char* element_ns, const char* element_name, const char* attribute_name);

float xml_get_attribute_float(xml_handle_t xml, const char* element_ns, const char* element_name, const char* attribute_name);

// if element name is not unique in xml, use below method to get element first, then get element's value or attribute
xml_element_t xml_get_element(xml_handle_t xml);

xml_element_t element_get_sibling(xml_element_t element);

xml_element_t element_get_child(xml_element_t element, const char* child_ns, const char* child_name);

const char* element_get_child_text(xml_element_t element, const char* child_ns, const char* child_name);

int element_get_child_int(xml_element_t element, const char* child_ns, const char* child_name);

float element_get_child_float(xml_element_t element, const char* child_ns, const char* child_name);

const char* element_get_text(xml_element_t element);

int element_get_int(xml_element_t element);

float element_get_float(xml_element_t element);

const char* element_get_attribute_text(xml_element_t element, const char* attribute_name);

int element_get_attribute_int(xml_element_t element, const char* attribute_name);

float element_get_attribute_float(xml_element_t element, const char* attribute_name);

// add and serialize
int xml_add_element(xml_handle_t xml, const char* parent_ns, const char* parent_name, const char* ns, const char* name, XML_VALUE_TYPE type, const void* value);

int xml_add_attribute(xml_handle_t xml, const char* element_ns, const char* element_name, const char* name, XML_VALUE_TYPE type, const void* value);

const char* xml_serialize(xml_handle_t xml);

// debug
void xml_debug_print(xml_handle_t xml);

#ifdef __cplusplus
}
#endif

#endif