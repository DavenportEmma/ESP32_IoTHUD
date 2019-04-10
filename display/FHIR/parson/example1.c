// example 1
#include "parson.c"
#include "parson.h"

void main()
{
	JSON_Value *root_value;
	JSON_Object *data;

	char *JSON_STRING  = "{\"name\":\"conor\",\"age\":22,\"admin\":true}";
	root_value = json_parse_string(JSON_STRING);
	data = json_value_get_object(root_value);
	printf("%s\n",json_object_get_string(data, "name"));
	printf("%f\n",json_object_get_number(data, "age"));
	printf("%d\n",json_object_get_boolean(data, "admin"));
}