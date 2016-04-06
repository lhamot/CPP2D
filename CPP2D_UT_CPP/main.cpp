#include "test.h"
#include "framework.h"

int main()
{
	check_operators();

	check_function();

	check_struct();

	check_static_array();

	check_dynamic_array();

	check_template_struct_specialization();

	check_template_function();

	check_class();

	check_type_alias();

	check_bitfield();

	check_decayed_type();

	check_injectedclassnametype();

	check_convertion_operator();

	check_abstract_keyword();

	check_class_opassign();

	check_tmpl_func_spec();

	check_variadic();

	check_tmpl_meth();

	check_function_pointer();

	check_method_pointer();

	check_member_pointer();

	check_uninstantiated_default_arg();

	check_class_instantiation();

	check_incomplete_array_type();

	check_builtin_macro();

	check_incr_pointer();

	check_function_macro();

	check_range_based_for_loop();

	check_overloaded_operator();

	check_extern_overloaded_operator();

	print_results();

	return 0;
}