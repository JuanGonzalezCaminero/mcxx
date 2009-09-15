/*
    Mercurium C/C++ Compiler
    Copyright (C) 2006-2009 - Roger Ferrer Ibanez <roger.ferrer@bsc.es>
    Barcelona Supercomputing Center - Centro Nacional de Supercomputacion
    Universitat Politecnica de Catalunya

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "tl-scope.hpp"
#include "cxx-scope.h"
#include "cxx-printscope.h"
#include "cxx-utils.h"
#include "hash_iterator.h"

namespace TL
{
    void Scope::printscope()
    {
        print_scope(_decl_context);
    }

    void Scope::convert_to_vector(scope_entry_list_t* entry_list, ObjectList<Symbol>& out)
    {
        while (entry_list != NULL)
        {
            Symbol s(entry_list->entry);
            out.push_back(s);
            entry_list = entry_list->next;
        }
    }

    void Scope::get_head(const ObjectList<Symbol>& in, Symbol& out)
    {
        if (in.size() > 0)
        {
            ObjectList<Symbol>::const_iterator it = in.begin();
            out = (*it);
        }
        else
        {
            out = Symbol::invalid();
        }
    }

    tl_type_t* Scope::get_extended_attribute(const std::string&) const
    {
        return NULL;
    }

    ObjectList<Symbol> Scope::get_symbols_from_name(const std::string& str) const
    {
        ObjectList<Symbol> result;
        // Fix this for C++
        scope_entry_list_t* entry_list = query_unqualified_name_str(_decl_context, const_cast<char*>(str.c_str()));

        convert_to_vector(entry_list, result);

        return result;
    }

    Symbol Scope::get_symbol_from_name(const std::string& str) const
    {
        ObjectList<Symbol> list = this->get_symbols_from_name(str);

        Symbol result(NULL);
        get_head(list, result);

        return result;
    }

    ObjectList<Symbol> Scope::get_symbols_from_id_expr(TL::AST_t ast) const
    {
        ObjectList<Symbol> result;
        AST _ast = ast._ast;

        scope_entry_list_t* entry_list = query_id_expression(_decl_context, _ast);

        convert_to_vector(entry_list, result);

        return result;
    }

    Symbol Scope::get_symbol_from_id_expr(TL::AST_t ast) const
    {
        ObjectList<Symbol> list = this->get_symbols_from_id_expr(ast);

        Symbol result(NULL);
        get_head(list, result);

        return result;
    }

    Scope Scope::temporal_scope() const
    {
        decl_context_t block_context = new_block_context(_decl_context);

        return Scope(block_context);
    }

    Scope& Scope::operator=(Scope sc)
    {
        this->_decl_context = sc._decl_context;
        return (*this);
    }

    bool Scope::operator<(Scope sc) const
    {
        return (this->_decl_context.current_scope < sc._decl_context.current_scope);
    }

    bool Scope::operator==(Scope sc) const
    {
        return (this->_decl_context.current_scope == sc._decl_context.current_scope);
    }

    bool Scope::operator!=(Scope sc) const
    {
        return !(this->operator==(sc));
    }

    ObjectList<Symbol> Scope::get_all_symbols(bool include_hidden)
    {
        ObjectList<Symbol> result;

        // This should be a bit more encapsulated in cxx-scope.c
        Iterator *it;

        it = (Iterator*) hash_iterator_create(_decl_context.current_scope->hash);
        for ( iterator_first(it); 
                !iterator_finished(it); 
                iterator_next(it))
        {
            scope_entry_list_t* entry_list = (scope_entry_list_t*) iterator_item(it);
            scope_entry_list_t* it = entry_list;

            while (it != NULL)
            {
                scope_entry_t* entry = it->entry;

                // Well, do_not_print is what we use to hide symbols :)
                if (!entry->do_not_print
                        || include_hidden)
                {
                    Symbol sym(entry);
                    result.append(sym);
                }

                it = it->next;
            }

        }

        return result;
    }

    Symbol Scope::new_artificial_symbol(const std::string& artificial_name)
    {
        scope_entry_list_t* sym_res_list = ::query_in_scope_str(_decl_context, artificial_name.c_str());
        scope_entry_t* sym_res = NULL;

        if (sym_res_list == NULL)
        {
            // Create the symbol
            sym_res = ::new_symbol(_decl_context, _decl_context.current_scope, artificial_name.c_str());
        }
        else
        {
            sym_res = sym_res_list->entry;
            if (sym_res->kind != SK_OTHER)
            {
                internal_error("This function can only be used for artificial symbols. '%s' is not artificial", sym_res->symbol_name);
            }
        }

        return Symbol(sym_res);
    }
}
