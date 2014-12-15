/*--------------------------------------------------------------------
  (C) Copyright 2006-2013 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion
  
  This file is part of Mercurium C/C++ source-to-source compiler.
  
  See AUTHORS file in the top level directory for information
  regarding developers and contributors.
  
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


#include "tl-counters.hpp"
#include "tl-lowering-visitor.hpp"
#include "tl-lowering-utils.hpp"
#include "tl-symbol-utils.hpp"
#include "tl-lower-reductions.hpp"


namespace TL { namespace Intel {

void LoweringVisitor::visit(const Nodecl::OpenMP::Parallel& construct)
{
    Nodecl::NodeclBase statements = construct.get_statements();
    walk(statements);
    statements = construct.get_statements(); // Should not be necessary

    Nodecl::List environment = construct.get_environment().as<Nodecl::List>();

    Nodecl::NodeclBase num_threads = construct.get_num_replicas();

    TL::ObjectList<Nodecl::OpenMP::Shared> shared_list = environment.find_all<Nodecl::OpenMP::Shared>();
    TL::ObjectList<Nodecl::OpenMP::Private> private_list = environment.find_all<Nodecl::OpenMP::Private>();
    TL::ObjectList<Nodecl::OpenMP::Firstprivate> firstprivate_list = environment.find_all<Nodecl::OpenMP::Firstprivate>();
    TL::ObjectList<Nodecl::OpenMP::Reduction> reduction_list = environment.find_all<Nodecl::OpenMP::Reduction>();

    TL::ObjectList<TL::Symbol> all_symbols_passed; // Set of all symbols passed in the outline
    TL::ObjectList<TL::Symbol> private_symbols;
    TL::ObjectList<TL::Symbol> firstprivate_symbols;
    TL::ObjectList<TL::Symbol> shared_symbols;
    TL::ObjectList<TL::Symbol> reduction_symbols;

    if (!shared_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            shared_list  // TL::ObjectList<OpenMP::Shared>
            .map(&Nodecl::OpenMP::Shared::get_symbols) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::get_symbol) // TL::ObjectList<TL::Symbol>
            ;

        shared_symbols.insert(tmp);
        all_symbols_passed.insert(tmp);
    }
    if (!private_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            private_list  // TL::ObjectList<OpenMP::Private>
            .map(&Nodecl::OpenMP::Private::get_symbols) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::get_symbol) // TL::ObjectList<TL::Symbol>
            ;

        private_symbols.insert(tmp);
    }
    if (!firstprivate_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            firstprivate_list  // TL::ObjectList<OpenMP::Firstprivate>
            .map(&Nodecl::OpenMP::Firstprivate::get_symbols) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::get_symbol) // TL::ObjectList<TL::Symbol>
            ;

        private_symbols.insert(tmp);
        firstprivate_symbols.insert(tmp);
        all_symbols_passed.insert(tmp);
    }

    // Add the VLA symbols
    {
        TL::ObjectList<TL::Symbol> vla_symbols;
        for (TL::ObjectList<TL::Symbol>::iterator it = all_symbols_passed.begin();
                it != all_symbols_passed.end();
                it++)
        {
            Intel::gather_vla_symbols(*it, vla_symbols);
        }

        // VLA symbols are always firstprivate
        private_symbols.insert(vla_symbols);
        firstprivate_symbols.insert(vla_symbols);

        // We want all the gathered VLA symbols be the first ones
        vla_symbols.insert(all_symbols_passed);
        all_symbols_passed = vla_symbols;
    }
    if (!reduction_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            reduction_list // TL::ObjectList<OpenMP::Reduction>
            .map(&Nodecl::OpenMP::Reduction::get_reductions) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map(&Nodecl::List::to_object_list_as<Nodecl::OpenMP::ReductionItem>)
            // TL::ObjectList<TL::ObjectList<Nodecl::OpenMP::ReductionItem> >
            .reduction
            (TL::append_two_lists<Nodecl::OpenMP::ReductionItem>) // TL::ObjectList<OpenMP::ReductionItem>
            .map(&Nodecl::OpenMP::ReductionItem::get_reduced_symbol) // TL::ObjectList<Nodecl::NodeclBase>
            .map(&Nodecl::NodeclBase::get_symbol); // TL::ObjectList<TL::Symbol>

        private_symbols.insert(tmp);
        reduction_symbols.insert(tmp);
        all_symbols_passed.insert(tmp);
    }

    TL::Type kmp_int32_type = Source("kmp_int32").parse_c_type_id(construct);
    ERROR_CONDITION(!kmp_int32_type.is_valid(), "Type kmp_int32 not in scope", 0);

    TL::Symbol enclosing_function = Nodecl::Utils::get_enclosing_function(construct);
    std::string outline_function_name;
    {
        TL::Counter &outline_num = TL::CounterManager::get_counter("intel-omp-outline");
        std::stringstream ss;
        ss << "_ol_" << enclosing_function.get_name() << "_" << (int)outline_num;
        outline_function_name = ss.str();
        outline_num++;
    }

    TL::ObjectList<std::string> parameter_names;
    TL::ObjectList<TL::Type> parameter_types;

    parameter_names.append("_global_tid"); parameter_types.append(kmp_int32_type.get_pointer_to());
    parameter_names.append("_bound_tid"); parameter_types.append(kmp_int32_type.get_pointer_to());

    for (TL::ObjectList<TL::Symbol>::iterator it = all_symbols_passed.begin();
            it != all_symbols_passed.end();
            it++)
    {
        parameter_names.append(it->get_name());
        parameter_types.append(it->get_type().no_ref().get_lvalue_reference_to());
    }

    TL::Symbol outline_function = SymbolUtils::new_function_symbol(
            enclosing_function,
            outline_function_name,
            TL::Type::get_void_type(),
            parameter_names,
            parameter_types);

    TL::Symbol ident_symbol = Intel::new_global_ident_symbol(construct);

    Nodecl::NodeclBase outline_function_code, outline_function_stmt;
    Nodecl::List reduction_code_list;
    SymbolUtils::build_empty_body_for_function(outline_function,
            outline_function_code,
            outline_function_stmt);

    Nodecl::Utils::SimpleSymbolMap symbol_map;

    TL::Scope block_scope = outline_function_stmt.retrieve_context();

    for (TL::ObjectList<TL::Symbol>::iterator it = all_symbols_passed.begin();
            it != all_symbols_passed.end();
            it++)
    {
        TL::Symbol parameter = block_scope.get_symbol_from_name(it->get_name());
        ERROR_CONDITION(!parameter.is_valid(), "Invalid symbol", 0);

        symbol_map.add_map(*it, parameter);
    }

    // Make an extra pass on the parameters to update their types in case of VLAs
    // We will use the symbol map computed above
    {
        // Update every parameter type
        TL::ObjectList<TL::Type> updated_parameter_types = outline_function.get_type().parameters();
        for (TL::ObjectList<TL::Type>::iterator it = updated_parameter_types.begin();
                it != updated_parameter_types.end();
                it++)
        {
            *it = ::type_deep_copy(it->get_internal_type(),
                    outline_function_stmt.retrieve_context().get_decl_context(),
                    symbol_map.get_symbol_map());
        }
        // Update the function type
        outline_function.set_type(
                TL::Type::get_void_type().get_function_returning(updated_parameter_types,
                    /* has_ellipsis */ false,
                    /* reference_qualifier */ REF_QUALIFIER_NONE));

        // Now update the type of the symbols of the parameters
        TL::ObjectList<TL::Symbol> parameter_symbols = outline_function.get_related_symbols();
        for (TL::ObjectList<TL::Symbol>::iterator it = parameter_symbols.begin();
                it != parameter_symbols.end();
                it++)
        {
            it->get_internal_symbol()->type_information = ::type_deep_copy(
                    it->get_internal_symbol()->type_information,
                    outline_function_stmt.retrieve_context().get_decl_context(),
                    symbol_map.get_symbol_map());
        }
    }

    for (TL::ObjectList<TL::Symbol>::iterator it = private_symbols.begin();
            it != private_symbols.end();
            it++)
    {
        TL::Symbol new_private_sym = Intel::new_private_symbol(*it, block_scope);

        new_private_sym.get_internal_symbol()->type_information = ::type_deep_copy(
                new_private_sym.get_internal_symbol()->type_information,
                outline_function_stmt.retrieve_context().get_decl_context(),
                symbol_map.get_symbol_map());

        if (firstprivate_symbols.contains(*it))
        {
            if (!new_private_sym.get_type().is_array())
            {
                new_private_sym.set_value(
                        symbol_map.map(*it).make_nodecl(/* set_ref_type */ true)
                        );
            }
            else
            {
                Source init_array;

                // FIXME - Use assignment instead
                init_array
                    << "__builtin_memcpy(" << as_symbol(new_private_sym) << ","
                    <<                        as_symbol(symbol_map.map(*it))
                    <<                        ", sizeof(" << as_symbol(*it) << "));"
                    ;

                Nodecl::NodeclBase init_array_tree = init_array.parse_statement(outline_function_stmt);
                outline_function_stmt.prepend_sibling(init_array_tree);
            }
        }


        // Will override privates.
        // Do not move before the if (firstprivate_symbols.contains(*it))
        // or the __builtin_memcpy will not work
        symbol_map.add_map(*it, new_private_sym);

        CXX_LANGUAGE()
        {
            outline_function_stmt.prepend_sibling(
                    Nodecl::CxxDef::make(
                        /* context */ Nodecl::NodeclBase::null(),
                        new_private_sym));
        }
    }

    Nodecl::NodeclBase parallel_body = Nodecl::Utils::deep_copy(statements,
            outline_function_stmt,
            symbol_map);

    outline_function_stmt.prepend_sibling(parallel_body);

    Nodecl::Utils::prepend_to_enclosing_top_level_location(construct, outline_function_code);
    

    // Reductions
    if (!reduction_list.empty())
    {
        TL::ObjectList<Nodecl::OpenMP::ReductionItem> reduction_items = reduction_list
            .map(&Nodecl::OpenMP::Reduction::get_reductions)
            .map(&Nodecl::NodeclBase::as<Nodecl::List>)
            .map(&Nodecl::List::to_object_list)
            .reduction(&TL::append_two_lists<Nodecl::NodeclBase>)
            .map(&Nodecl::NodeclBase::as<Nodecl::OpenMP::ReductionItem>);

        for (TL::ObjectList<Nodecl::OpenMP::ReductionItem>::iterator it = reduction_items.begin();
                it != reduction_items.end();
                it++)
        {
            Nodecl::OpenMP::ReductionItem &current(*it);

            TL::Symbol reductor = current.get_reductor().get_symbol();
            OpenMP::Reduction* omp_reduction = OpenMP::Reduction::get_reduction_info_from_symbol(reductor);

            TL::Symbol reduced_symbol = current.get_reduced_symbol().get_symbol();

            TL::Symbol private_symbol = symbol_map.map(reduced_symbol);

            // The reduction must be performed onto the shared variable. We have it in
            // the parameters, rather than in reduced_symbol that it is only the private copy
            TL::Symbol parameter_symbol = block_scope.get_symbol_from_name(reduced_symbol.get_name());
            ERROR_CONDITION (!parameter_symbol.is_valid(), "Reduction shared symbol not found", 0);

            // FIXME - We should actually update the initializer for omp_orig and omp_priv
            private_symbol.set_value(omp_reduction->get_initializer().shallow_copy());
            outline_function_stmt.prepend_sibling(
                    Nodecl::ObjectInit::make(private_symbol)
                    );

            TL::Symbol callback = emit_callback_for_reduction(omp_reduction, construct, enclosing_function);

            Nodecl::Utils::SimpleSymbolMap combiner_map;
            combiner_map.add_map(omp_reduction->get_omp_out(), parameter_symbol);
            combiner_map.add_map(omp_reduction->get_omp_in(), private_symbol);

            Source master_combiner;
            master_combiner
                << as_expression(
                        Nodecl::Utils::deep_copy(omp_reduction->get_combiner(), construct, combiner_map)
                        ) << ";";

            Source reduction_src;
            reduction_src
                << "switch (__kmpc_reduce_nowait(&" << as_symbol(ident_symbol)
                <<               ", __kmpc_global_thread_num(&" << as_symbol(ident_symbol) << ")"
                <<               ", 1"
                <<               ", sizeof(" << as_type(omp_reduction->get_type()) << ")"
                <<               ", &" << as_symbol(private_symbol)
                <<               ", (void(*)(void*,void*))" << as_symbol(callback)
                <<               ", &" << as_symbol(Intel::get_global_lock_symbol(construct)) << "))"
                << "{"
                <<    "case 1:"
                <<    "{"
                <<       master_combiner
                <<       "__kmpc_end_reduce_nowait(&" << as_symbol(ident_symbol)
                <<               ", __kmpc_global_thread_num(&" << as_symbol(ident_symbol) << ")"
                <<               ", &" << as_symbol(Intel::get_global_lock_symbol(construct)) << ");"
                <<       "break;"
                <<    "}"
                <<    "case 0: break;"
                <<    "default: __builtin_abort();"
                << "}"
                ;

            Nodecl::NodeclBase reduction_tree = reduction_src.parse_statement(outline_function_stmt);
            reduction_code_list.append(reduction_tree);
        }
    }

    outline_function_stmt.append_sibling(reduction_code_list);

    // Spawn
    TL::Type kmp_micro_type = Source("kmpc_micro").parse_c_type_id(construct);

    Source fork_call;

    if (!num_threads.is_null())
    {
        fork_call
            << "__kmpc_push_num_threads(&" << as_symbol(ident_symbol) << ", "
            <<               "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
            <<               as_expression(num_threads.shallow_copy()) << ");"
        ;
    }

    fork_call
        << "__kmpc_fork_call(&" << as_symbol(ident_symbol) << ", "
        <<                           all_symbols_passed.size() << ", (" << as_type(kmp_micro_type) << ")"
        <<                           as_symbol(outline_function)
        ;
    for (TL::ObjectList<TL::Symbol>::iterator it = all_symbols_passed.begin();
            it != all_symbols_passed.end();
            it++)
    {
        if (!it->get_type().is_array())
        {
            fork_call << ", &" << as_symbol(*it);
        }
        else
        {
            fork_call << ", " << as_symbol(*it);
        }
    }
    fork_call << ");";

    Nodecl::NodeclBase fork_call_tree = fork_call.parse_statement(construct);

    construct.replace(fork_call_tree);
}

} }
