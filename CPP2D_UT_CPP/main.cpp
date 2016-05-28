//
// Copyright (c) 2016 Loïc HAMOT
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
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

	check_lib_porting_pair();

	check_union();

	check_lambda();

	check_struct_default_ctor();

	check_struct_ctor_call();

	check_class_ctor_call();

	check_exception();

	check_exception2();

	check_for_loop();

	check_while_loop();

	check_dowhile_loop();

	check_multidecl_line();

	check_std_array();

	check_implicit_ctor();

	check_extern_methode();

	check_std_unordered_map();

	check_struct_containing_scooped_class();

	check_not_copyable_class();

	check_break();

	check_continue();

	check_switch();

	print_results();

	return 0;
}