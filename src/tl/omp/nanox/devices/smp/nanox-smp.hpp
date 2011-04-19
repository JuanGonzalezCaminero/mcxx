/*--------------------------------------------------------------------
  (C) Copyright 2006-2011 Barcelona Supercomputing Center 
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


#ifndef NANOX_SMP_HPP
#define NANOX_SMP_HPP

#include "tl-compilerphase.hpp"
#include "tl-devices.hpp"
#include "tl-simd.hpp"

namespace TL
{

    namespace Nanox
    {

        class DeviceSMP : public DeviceProvider
        {
            public:

                virtual void run(DTO& dto);
                virtual void pre_run(DTO& dto);

                DeviceSMP();

                virtual ~DeviceSMP() { }

                virtual void create_outline(
                        const std::string& task_name,
                        const std::string& struct_typename,
                        DataEnvironInfo &data_environ,
                        const OutlineFlags& outline_flags,
                        AST_t reference_tree,
                        ScopeLink sl,
                        Source initial_setup,
                        Source outline_body);

                virtual void do_replacements(DataEnvironInfo& data_environ,
                        AST_t body,
                        ScopeLink scope_link,
                        Source &initial_setup,
                        Source &replace_src);

                virtual void get_device_descriptor(const std::string& task_name,
                        DataEnvironInfo &data_environ,
                        const OutlineFlags& outline_flags,
                        AST_t reference_tree,
                        ScopeLink sl,
                        Source &ancillary_device_description,
                        Source &device_descriptor);
                        
                virtual Source get_reduction_update(ObjectList<OpenMP::ReductionSymbol> reduction_references, ScopeLink sl);
                virtual Source get_reduction_code(ObjectList<OpenMP::ReductionSymbol> reduction_references, ScopeLink sl);

            private:
                void do_smp_inline_get_addresses(
                        const Scope& sc,
                        const DataEnvironInfo& data_env_info,
                        Source &copy_setup,
                        ReplaceSrcIdExpression& replace_src,
                        bool &err_declared);

                void do_smp_outline_replacements(AST_t body,
                        ScopeLink scope_link,
                        const DataEnvironInfo& data_env_info,
                        Source &initial_code,
                        Source &replaced_outline);
        };

        class ReplaceSrcSMP : public TL::SIMD::ReplaceSrcGenericFunction
        {
            protected:
                static const char* prettyprint_callback (AST a, void* data);
                static const char* recursive_prettyprint (AST_t a, void* data);
                static std::string get_integer_casting(AST_t a, Type type1, Type type2);
                static std::string scalar_expansion(Expression exp);

            public:
                ReplaceSrcSMP(ScopeLink sl, int width) 
                    : ReplaceSrcGenericFunction(sl, "smp", width){}

                void add_replacement(Symbol sym, const std::string& str);
                void add_this_replacement(const std::string& str);
                Source replace(AST_t a) const;
                Source replace_naive_function(const Symbol& func_sym, const std::string& naive_func_name, const ScopeLink sl);
                Source replace_simd_function(const Symbol& func_sym, const std::string& simd_func_name, const ScopeLink sl);
        };
    }
}

#endif // NANOX_SMP_HPP
