/*--------------------------------------------------------------------
  (C) Copyright 2006-2009 Barcelona Supercomputing Center 
                          Centro Nacional de Supercomputacion
  
  This file is part of Mercurium C/C++ source-to-source compiler.
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.
  
  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/

#include <string.h>
#include "cxx-utils.h"
#include "cxx-ast.h"
#include "cxx-solvetemplate.h"
#include "cxx-instantiation.h"
#include "cxx-prettyprint.h"
#include "cxx-buildscope.h"
#include "cxx-typeutils.h"
#include "cxx-cexpr.h"
#include "cxx-ambiguity.h"

#include "cxx-printscope.h"

static const char* get_name_of_template_parameter(
        template_parameter_list_t* template_parameters,
        int nesting,
        int position)
{
    int i;
    for (i = 0; i < template_parameters->num_template_parameters; i++)
    {
        template_parameter_t* current_template_parameter 
            = template_parameters->template_parameters[i];

        if ((current_template_parameter->entry->entity_specs.template_parameter_nesting == nesting)
                && (current_template_parameter->entry->entity_specs.template_parameter_position == position))
        {
            return current_template_parameter->entry->symbol_name;
        }
    }

    internal_error("Not found template parameter with nest=%d and position=%d",
            nesting, position);
}

static void instantiate_specialized_template_class(type_t* selected_template,
        type_t* being_instantiated,
        deduction_set_t* unification_set,
        const char *filename, int line)
{
    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: About to instantiate class '%s'\n", 
                named_type_get_symbol(being_instantiated)->symbol_name);
    }
    ERROR_CONDITION(!is_named_class_type(being_instantiated), "Must be a named class", 0);

    scope_entry_t* named_class = named_type_get_symbol(being_instantiated);

    AST instantiation_body = NULL;
    AST instantiation_base_clause = NULL;
    class_type_get_instantiation_trees(get_actual_class_type(selected_template), 
            &instantiation_body, &instantiation_base_clause);

    template_parameter_list_t* selected_template_parameters 
        = template_specialized_type_get_template_parameters(get_actual_class_type(selected_template));

    instantiation_body = ast_copy_for_instantiation(instantiation_body);
    instantiation_base_clause = ast_copy_for_instantiation(instantiation_base_clause);

    decl_context_t template_parameters_context = new_template_context(named_class->decl_context);
    // Clear the template nesting level, if we are instantiating it is conceptually 0
    template_parameters_context.decl_flags &= ~DF_TEMPLATE;
    template_parameters_context.decl_flags &= ~DF_EXPLICIT_SPECIALIZATION;
    template_parameters_context.template_nesting = 0;
    template_parameters_context.template_parameters = NULL;

    decl_context_t inner_decl_context = new_class_context(template_parameters_context, 
            /* FIXME the qualification name should be more useful */named_class->symbol_name,
            named_class->type_information);

    class_type_set_inner_context(named_class->type_information, inner_decl_context);

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Injecting template parameters\n");
    }

    int i;
    for (i = 0; i < unification_set->num_deductions; i++)
    {
        deduction_t* current_deduction = unification_set->deduction_list[i];

        ERROR_CONDITION(current_deduction->num_deduced_parameters != 1,
                "Number of deduced parameters is not 1!", 0);

        int j;
        for (j = 0; j < current_deduction->num_deduced_parameters; j++)
        {
            const char* deduced_parameter_name = get_name_of_template_parameter(selected_template_parameters,
                    current_deduction->parameter_nesting,
                    current_deduction->parameter_position);
            switch (current_deduction->kind)
            {
                case TPK_TYPE :
                    {
                        // Note that we sign in the symbol in template_scope and not in current_scope
                        scope_entry_t* injected_type = new_symbol(template_parameters_context, 
                                template_parameters_context.template_scope, deduced_parameter_name);

                        // We use a typedef
                        injected_type->kind = SK_TYPEDEF;
                        injected_type->entity_specs.is_template_argument = 1;
                        injected_type->type_information = get_new_typedef(current_deduction->deduced_parameters[0]->type);
                        break;
                    }
                case TPK_TEMPLATE :
                    {
                        // Note that we sign in the symbol in template_scope and not in current_scope
                        scope_entry_t* injected_type = new_symbol(template_parameters_context, 
                                template_parameters_context.template_scope, deduced_parameter_name);

                        // The template type has to be used here
                        injected_type->kind = SK_TEMPLATE;
                        injected_type->entity_specs.is_template_argument = 1;
                        // These are always kept as named types in the compiler
                        injected_type->type_information = 
                            named_type_get_symbol(current_deduction->deduced_parameters[0]->type)->type_information;
                        break;
                    }
                case TPK_NONTYPE :
                    {
                        scope_entry_t* injected_nontype = new_symbol(template_parameters_context, 
                                template_parameters_context.template_scope, deduced_parameter_name);

                        injected_nontype->kind = SK_VARIABLE;
                        injected_nontype->entity_specs.is_template_argument = 1;
                        injected_nontype->type_information = current_deduction->deduced_parameters[0]->type;

                        // Fold it, as makes things easier
                        literal_value_t literal_value = evaluate_constant_expression(current_deduction->deduced_parameters[0]->expression,
                                current_deduction->deduced_parameters[0]->decl_context);
                        AST evaluated_tree = tree_from_literal_value(literal_value);
                        AST fake_initializer = evaluated_tree;
                        injected_nontype->expression_value = fake_initializer;
                        break;
                    }
                default:
                    {
                        internal_error("Invalid parameter kind", 0);
                    }
            }
        }

    }

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Template parameters injected\n");
    }

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Injected context\n");
        print_scope(inner_decl_context);
        fprintf(stderr, "INSTANTIATION: End of injected context \n");
    }

    enter_class_specifier();

    if (instantiation_base_clause != NULL)
    {
        build_scope_base_clause(instantiation_base_clause, 
                get_actual_class_type(being_instantiated), 
                inner_decl_context);
    }
    
    // Inject the class name
    scope_entry_t* injected_symbol = new_symbol(inner_decl_context, 
            inner_decl_context.current_scope, named_class->symbol_name);

    *injected_symbol = *named_class;

    injected_symbol->do_not_print = 1;
    injected_symbol->entity_specs.is_injected_class_name = 1;
    injected_symbol->entity_specs.injected_class_referred_symbol = named_class;

    /*
     * Note that the standard allows code like this one
     *
     * template <typename _T>
     * struct A { };
     *
     * template <>
     * struct A<int> 
     * {
     *   typedef int K;  (1)
     *   A<int>::K k;    (2)
     * };
     *
     * So we can use the name 'A<int>::K' inside the class provided 'K' has been declared
     * before the point we refer it (so: switching declarations (1) and (2) would not work).
     *
     * This also affects the injected class-name, so, for another example
     *
     * template <typename _T>
     * struct B
     * {
     *   typedef _T P;             (3)
     *   typename B::P k;          (4)
     * };
     *
     * Note that in a partial, or not at all, specialized class the injected
     * class name is dependent so 'typename' is mandatory (like in (4)). It is
     * redundant since the injected-class name obviously refers to the current
     * class so no ambiguity would arise between identifiers and typenames.
     * Seems that a DR has been filled on that.
     *
     * All this explanation is here just to justify that the class is already
     * complete and independent just before start the parsing of its members.
     * Otherwise all the machinery would try to instantiate it again and again
     * (and this is not good at all).
     */
    class_type_set_complete_independent(get_actual_class_type(being_instantiated));

    if (instantiation_body != NULL)
    {
        inner_decl_context.decl_flags |= DF_INSTANTIATING;

        // Fix this AS_PUBLIC one day
        build_scope_member_specification_first_step(inner_decl_context, instantiation_body, AS_PUBLIC,
                being_instantiated);
    }

    // The symbol is defined after this
    named_class->defined = 1;

    leave_class_specifier();
    
    // Finish the class
    finish_class_type(get_actual_class_type(being_instantiated), being_instantiated, 
            named_class->decl_context, filename, line);

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Instantiation ended\n");
    }
}

void instantiate_template_class(scope_entry_t* entry, decl_context_t decl_context, const char *filename, int line)
{
    if (entry->kind != SK_CLASS
            && entry->kind != SK_TYPEDEF)
    {
        internal_error("Invalid symbol\n", 0);
    }

    if (entry->kind == SK_TYPEDEF)
    {
        entry = named_type_get_symbol(advance_over_typedefs(entry->type_information));
    }

    type_t* template_specialized_type = entry->type_information;

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Instantiating class '%s' at '%s:%d'\n",
                entry->symbol_name,
                entry->file,
                entry->line);
    }


    if (!is_template_specialized_type(template_specialized_type)
            || !is_class_type(template_specialized_type)
            || !class_type_is_incomplete_independent(template_specialized_type))
    {
        internal_error("Symbol '%s' is not a class eligible for instantiation", entry->symbol_name);
    }

    type_t* template_type = 
        template_specialized_type_get_related_template_type(template_specialized_type);

    deduction_set_t* unification_set = NULL;

    type_t* selected_template = solve_class_template(decl_context,
            template_type, 
            get_user_defined_type(entry),
            &unification_set, filename, line);

    if (selected_template != NULL)
    {
        instantiate_specialized_template_class(selected_template, 
                get_user_defined_type(entry),
                unification_set, filename, line);
    }
    else
    {
        internal_error("Could not instantiate template '%s' declared for first time in '%s:%d'", 
                entry->symbol_name,
                entry->file,
                entry->line);
    }
}

decl_context_t get_instantiation_context(scope_entry_t* entry, template_parameter_list_t* template_parameters)
{
    type_t* template_specialized_type = entry->type_information;

    if (!is_template_specialized_type(template_specialized_type)
            || !is_function_type(template_specialized_type))
    {
        internal_error("Symbol '%s' is not a template function eligible for instantiation", entry->symbol_name);
    }

    // type_t* template_type = template_specialized_type_get_related_template_type(template_specialized_type);
    // scope_entry_t* template_symbol = template_type_get_related_symbol(template_type);

    // The primary specialization is a named type, even if the named type is a function!
    // type_t* primary_specialization_type = template_type_get_primary_type(template_symbol->type_information);
    // scope_entry_t* primary_specialization_function = named_type_get_symbol(primary_specialization_type);
    // type_t* primary_specialization_function_type = primary_specialization_function->type_information;

    // Functions are easy. Since they cannot be partially specialized, like
    // classes do, their template parameters always match the computed template
    // arguments. In fact, overload machinery did this part for us

    // Get a new template context where we will sign in the template arguments
    decl_context_t template_parameters_context = new_template_context(entry->decl_context);

    if (template_parameters == NULL)
    {
        template_parameters = template_specialized_type_get_template_parameters(template_specialized_type);
    }

    template_argument_list_t *template_arguments
        = template_specialized_type_get_template_arguments(template_specialized_type);

    // FIXME: Does this hold when in C++0x we allow default template parameters in functions?
    ERROR_CONDITION(template_arguments->num_arguments != template_parameters->num_template_parameters,
            "Mismatch between template arguments and parameters! %d != %d\n", 
            template_arguments->num_arguments,
            template_parameters->num_template_parameters);

    DEBUG_CODE()
    {
        fprintf(stderr, "NUM TEMPLATE PARAMS %d\n", template_parameters->num_template_parameters);
    }

    // Inject template arguments
    int i;
    for (i = 0; i < template_parameters->num_template_parameters; i++)
    {
        template_parameter_t* template_param = template_parameters->template_parameters[i];
        template_argument_t* template_argument = template_arguments->argument_list[i];

        switch (template_param->kind)
        {
            case TPK_TYPE:
                {
                    ERROR_CONDITION(template_argument->kind != TAK_TYPE,
                            "Mismatch between template argument kind and template parameter kind", 0);

                    scope_entry_t* injected_type = new_symbol(template_parameters_context,
                            template_parameters_context.template_scope, template_param->entry->symbol_name);

                    DEBUG_CODE()
                    {
                        fprintf(stderr, "Injecting typedef '%s' to '%s'\n", injected_type->symbol_name,
                                print_declarator(template_argument->type));
                    }

                    injected_type->kind = SK_TYPEDEF;
                    injected_type->entity_specs.is_template_argument = 1;
                    injected_type->type_information = get_new_typedef(template_argument->type);
                    break;
                }
            case TPK_TEMPLATE:
                {
                    ERROR_CONDITION(template_argument->kind != TAK_TEMPLATE,
                            "Mismatch between template argument kind and template parameter kind", 0);

                    scope_entry_t* injected_type = new_symbol(template_parameters_context, 
                            template_parameters_context.template_scope, template_param->entry->symbol_name);

                    injected_type->kind = SK_TEMPLATE;
                    injected_type->entity_specs.is_template_argument = 1;
                    injected_type->type_information = 
                        named_type_get_symbol(template_argument->type)->type_information;

                    DEBUG_CODE()
                    {
                        fprintf(stderr, "Injecting template name '%s'\n", injected_type->symbol_name);
                    }

                    break;
                }
            case TPK_NONTYPE:
                {
                    ERROR_CONDITION(template_argument->kind != TAK_NONTYPE,
                            "Mismatch between template argument kind and template parameter kind", 0);

                    scope_entry_t* injected_nontype = new_symbol(template_parameters_context, 
                            template_parameters_context.template_scope, template_param->entry->symbol_name);

                    injected_nontype->kind = SK_VARIABLE;
                    injected_nontype->entity_specs.is_template_argument = 1;
                    injected_nontype->type_information = template_argument->type;

                    // Fold it, as makes things easier
                    literal_value_t literal_value = evaluate_constant_expression(template_argument->expression,
                            template_argument->expression_context);
                    AST evaluated_tree = tree_from_literal_value(literal_value);
                    AST fake_initializer = evaluated_tree;
                    injected_nontype->expression_value = fake_initializer;

                    DEBUG_CODE()
                    {
                        fprintf(stderr, "Injecting parameter '%s' with expression '%s'\n", 
                                injected_nontype->symbol_name,
                                prettyprint_in_buffer(fake_initializer));
                    }
                    break;
                }
            default:
                internal_error("Invalid template parameter kind %d\n", template_param->kind);
        }
    }

    return template_parameters_context;
}
 
void instantiate_template_function(scope_entry_t* entry, 
        decl_context_t decl_context UNUSED_PARAMETER, 
        const char* filename UNUSED_PARAMETER, 
        int line UNUSED_PARAMETER)
{
    // Do nothing if we are checking for ambiguities as this may cause havoc
    if (checking_ambiguity())
        return;

    if (entry->kind != SK_FUNCTION)
    {
        internal_error("Invalid symbol\n", 0);
    }

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Instantiating function '%s' with type '%s' at '%s:%d\n",
                entry->symbol_name,
                print_declarator(entry->type_information),
                entry->file,
                entry->line);
    }

    type_t* template_specialized_type = entry->type_information;

    if (!is_template_specialized_type(template_specialized_type)
            || !is_function_type(template_specialized_type))
    {
        internal_error("Symbol '%s' is not a template function eligible for instantiation", entry->symbol_name);
    }

    type_t* template_type = template_specialized_type_get_related_template_type(template_specialized_type);
    scope_entry_t* template_symbol = template_type_get_related_symbol(template_type);

    // The primary specialization is a named type, even if the named type is a function!
    type_t* primary_specialization_type = template_type_get_primary_type(template_symbol->type_information);
    scope_entry_t* primary_specialization_function = named_type_get_symbol(primary_specialization_type);
    type_t* primary_specialization_function_type = primary_specialization_function->type_information;

    // If the primary specialization is not defined, no instantiation may happen
    if (!primary_specialization_function->defined)
    {
        DEBUG_CODE()
        {
            fprintf(stderr, "INSTANTIATION: Not instantiating since primary template has not been defined\n");
        }
        return;
    }

    // Do nothing
    if (entry->defined)
    {
        DEBUG_CODE()
        {
            fprintf(stderr, "INSTANTIATION: Instantiation already performed\n");
        }
        return;
    }

    // Functions are easy. Since they cannot be partially specialized, like
    // classes do, their template parameters always match the computed template
    // arguments. In fact, overload machinery did this part for us

    // Get a new template context where we will sign in the template arguments
    decl_context_t template_parameters_context = new_template_context(entry->decl_context);

    template_parameter_list_t *template_parameters 
        = template_specialized_type_get_template_parameters(template_specialized_type);

    template_argument_list_t *template_arguments
        = template_specialized_type_get_template_arguments(template_specialized_type);

    // FIXME: Does this hold when in C++0x we allow default template parameters in functions?
    ERROR_CONDITION(template_arguments->num_arguments != template_parameters->num_template_parameters,
            "Mismatch between template arguments and parameters! %d != %d\n", 
            template_arguments->num_arguments,
            template_parameters->num_template_parameters);

    // Inject template arguments
    int i;
    for (i = 0; i < template_parameters->num_template_parameters; i++)
    {
        template_parameter_t* template_param = template_parameters->template_parameters[i];
        template_argument_t* template_argument = template_arguments->argument_list[i];

        switch (template_param->kind)
        {
            case TPK_TYPE:
                {
                    ERROR_CONDITION(template_argument->kind != TAK_TYPE,
                            "Mismatch between template argument kind and template parameter kind", 0);

                    scope_entry_t* injected_type = new_symbol(template_parameters_context,
                            template_parameters_context.template_scope, template_param->entry->symbol_name);

                    DEBUG_CODE()
                    {
                        fprintf(stderr, "Injecting typedef '%s' for type '%s'\n", injected_type->symbol_name, 
                                print_declarator(template_argument->type));
                    }

                    injected_type->kind = SK_TYPEDEF;
                    injected_type->entity_specs.is_template_argument = 1;
                    injected_type->type_information = get_new_typedef(template_argument->type);
                    break;
                }
            case TPK_TEMPLATE:
                {
                    ERROR_CONDITION(template_argument->kind != TAK_TEMPLATE,
                            "Mismatch between template argument kind and template parameter kind", 0);

                    scope_entry_t* injected_type = new_symbol(template_parameters_context, 
                            template_parameters_context.template_scope, template_param->entry->symbol_name);

                    DEBUG_CODE()
                    {
                        fprintf(stderr, "Injecting template name '%s'\n", injected_type->symbol_name);
                    }

                    injected_type->kind = SK_TEMPLATE;
                    injected_type->entity_specs.is_template_argument = 1;
                    injected_type->type_information = 
                        named_type_get_symbol(template_argument->type)->type_information;
                    break;
                }
            case TPK_NONTYPE:
                {
                    ERROR_CONDITION(template_argument->kind != TAK_NONTYPE,
                            "Mismatch between template argument kind and template parameter kind", 0);

                    scope_entry_t* injected_nontype = new_symbol(template_parameters_context, 
                            template_parameters_context.template_scope, template_param->entry->symbol_name);

                    injected_nontype->kind = SK_VARIABLE;
                    injected_nontype->entity_specs.is_template_argument = 1;
                    injected_nontype->type_information = template_argument->type;


                    // Fold it, as makes things easier
                    literal_value_t literal_value = evaluate_constant_expression(template_argument->expression,
                            template_argument->expression_context);
                    AST evaluated_tree = tree_from_literal_value(literal_value);
                    AST fake_initializer = evaluated_tree;
                    injected_nontype->expression_value = fake_initializer;

                    DEBUG_CODE()
                    {
                        fprintf(stderr, "Injecting parameter '%s' with expression '%s'\n", injected_nontype->symbol_name, 
                                prettyprint_in_buffer(fake_initializer));
                    }
                    break;
                }
            default:
                internal_error("Invalid template parameter kind %d\n", template_param->kind);
        }
    }

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: Getting tree of function '%s' || %s\n", entry->symbol_name, print_declarator(primary_specialization_function_type));
    }

    AST orig_function_definition = function_type_get_function_definition_tree(primary_specialization_function_type);

    ERROR_CONDITION(orig_function_definition == NULL,
            "Invalid function definition tree!", 0);

    // Remove dependent types
    AST dupl_function_definition = ast_copy_for_instantiation(orig_function_definition);

    template_parameters_context.decl_flags |= DF_TEMPLATE;
    template_parameters_context.decl_flags |= DF_EXPLICIT_SPECIALIZATION;

    // Temporarily disable ambiguity testing
    char old_test_status = get_test_expression_status();
    set_test_expression_status(0);

    build_scope_function_definition(dupl_function_definition,
            template_parameters_context);

    set_test_expression_status(old_test_status);

    DEBUG_CODE()
    {
        fprintf(stderr, "INSTANTIATION: ended instantation of function template '%s'\n",
                print_declarator(template_specialized_type));
    }
}


