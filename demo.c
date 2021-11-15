#include <stdio.h>
#include "xml.h"

int main()
{
    int ret = 0;
    char data[10] = {0};
    xml_handle_t handle_test = NULL;

    FILE* file = fopen("./test.xml", "r");
    if (file) {
        handle_test = xml_malloc_handle();
        if (handle_test == NULL) {
            printf("malloc failed");
            return -1;
        }
        do
        {
            ret = fread(data, 1, sizeof(data), file);
            if (ret > 0) {
                if (xml_input_raw(handle_test, data, ret) != 0) {
                    printf("an error occurred\n");
                    return -1;
                }
            }
        } while (ret > 0);
        fclose(file);
        xml_debug_print(handle_test);
        xml_free_handle(handle_test);
    } else {
        printf("cant not open test file\n");
    }

    return 0;
}