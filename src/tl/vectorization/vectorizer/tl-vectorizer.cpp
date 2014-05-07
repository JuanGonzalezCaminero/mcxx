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

#include "tl-vectorizer.hpp"

#include "cxx-cexpr.h"
#include "tl-source.hpp"
#include "tl-optimizations.hpp"

#include "tl-vectorization-analysis-interface.hpp"
#include "tl-vectorizer-loop-info.hpp"
#include "tl-vectorizer-visitor-preprocessor.hpp"
#include "tl-vectorizer-visitor-postprocessor.hpp"
#include "tl-vectorizer-visitor-for.hpp"
#include "tl-vectorizer-visitor-statement.hpp"
#include "tl-vectorizer-visitor-function.hpp"
#include "tl-vectorizer-vector-reduction.hpp"

namespace TL
{
namespace Vectorization
{
    Vectorizer *Vectorizer::_vectorizer = 0;
    FunctionVersioning Vectorizer::_function_versioning;

    Vectorizer& Vectorizer::get_vectorizer()
    {
        if(_vectorizer == 0)
            _vectorizer = new Vectorizer();

        return *_vectorizer;
    }

    void Vectorizer::initialize_analysis(
            const Nodecl::FunctionCode& enclosing_function)
    {
        VectorizationAnalysisInterface::initialize_analysis(enclosing_function);
    }

    void Vectorizer::finalize_analysis()
    {
        VectorizationAnalysisInterface::finalize_analysis();
    }


    Vectorizer::Vectorizer() : _avx2_enabled(false), _knc_enabled(false),
    _svml_sse_enabled(false), _svml_avx2_enabled(false), _svml_knc_enabled(false),
    _fast_math_enabled(false)
    {
    }

    Vectorizer::~Vectorizer()
    {
    }

    void Vectorizer::preprocess_code(const Nodecl::NodeclBase& n,
            const VectorizerEnvironment& environment)
    {
        VectorizerVisitorPreprocessor vectorizer_preproc(environment);
        vectorizer_preproc.walk(n);

        TL::Optimizations::canonicalize_and_fold(n, _fast_math_enabled);
        TL::Optimizations::canonicalize_and_fold(environment._suitable_expr_list,
                _fast_math_enabled);
    }

    void Vectorizer::postprocess_code(const Nodecl::NodeclBase& n)
    {
        TL::Optimizations::canonicalize_and_fold(n, _fast_math_enabled);

        VectorizerVisitorPostprocessor vectorizer_postproc;
        vectorizer_postproc.walk(n);

        TL::Optimizations::canonicalize_and_fold(n, _fast_math_enabled);
    }

    void Vectorizer::vectorize(Nodecl::ForStatement& for_statement,
            VectorizerEnvironment& environment)
    {
        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "VECTORIZER: ----- Vectorizing main ForStatement -----\n");
        }

        VectorizerVisitorFor visitor_for(environment);
        visitor_for.walk(for_statement);

        // Applying strenth reduction
        TL::Optimizations::canonicalize_and_fold(for_statement, _fast_math_enabled);

        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "\n");
        }
    }

    void Vectorizer::vectorize(Nodecl::FunctionCode& func_code,
            VectorizerEnvironment& environment,
            const bool masked_version)
    {
        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "VECTORIZER: ----- Vectorizing function code -----\n");
        }

        VectorizerVisitorFunction visitor_function(environment, masked_version);
        visitor_function.walk(func_code);

        // Applying strenth reduction
        TL::Optimizations::canonicalize_and_fold(func_code, _fast_math_enabled);

        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "\n");
        }
    }

    void Vectorizer::vectorize_parallel(Nodecl::NodeclBase& statements,
            VectorizerEnvironment& environment)
    {
        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "VECTORIZER: ----- Vectorizing Parallel -----\n");
        }

        VectorizerVisitorStatement visitor_stmt(environment, 
                /* cache enabled */ true);
        visitor_stmt.walk(statements);

        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "\n");
        }
    }

    void Vectorizer::process_epilog(Nodecl::ForStatement& for_statement,
            VectorizerEnvironment& environment,
            Nodecl::NodeclBase& net_epilog_node,
            int epilog_iterations,
            bool only_epilog,
            bool is_parallel_loop)
    {
        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "VECTORIZER: ----- Vectorizing epilog -----\n");
        }

        VectorizerVisitorForEpilog visitor_epilog(environment,
                epilog_iterations, only_epilog, is_parallel_loop);
        visitor_epilog.visit(for_statement, net_epilog_node);

        // Applying strenth reduction
        TL::Optimizations::canonicalize_and_fold(for_statement, _fast_math_enabled);
        TL::Optimizations::canonicalize_and_fold(net_epilog_node, _fast_math_enabled);

        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "\n");
        }
    }

    bool Vectorizer::is_supported_reduction(bool is_builtin,
            const std::string& reduction_name,
            const TL::Type& reduction_type,
            const VectorizerEnvironment& environment)
    {
        VectorizerVectorReduction vector_reduction(environment);

        return vector_reduction.is_supported_reduction(is_builtin,
                reduction_name, reduction_type);
    }

    int Vectorizer::get_epilog_info(const Nodecl::ForStatement& for_statement,
            VectorizerEnvironment& environment,
            bool& only_epilog)
    {
        VectorizerLoopInfo loop_info(for_statement, environment);

        return loop_info.get_epilog_info(for_statement,
                environment, only_epilog);
    }

    void Vectorizer::vectorize_reduction(const TL::Symbol& scalar_symbol,
            TL::Symbol& vector_symbol,
            const Nodecl::NodeclBase& initializer,
            const std::string& reduction_name,
            const TL::Type& reduction_type,
            const VectorizerEnvironment& environment,
            Nodecl::List& pre_nodecls,
            Nodecl::List& post_nodecls)
    {
        VectorizerVectorReduction vector_reduction(environment);

        vector_reduction.vectorize_reduction(
                scalar_symbol,
                vector_symbol,
                initializer,
                reduction_name,
                reduction_type,
                pre_nodecls,
                post_nodecls);
    }

    void Vectorizer::add_vector_function_version(const std::string& func_name,
            const Nodecl::NodeclBase& func_version,
            const std::string& device, const unsigned int vector_length,
            const TL::Type& target_type, const bool masked, const FunctionPriority priority,
            const bool is_svml)
    {
        VECTORIZATION_DEBUG()
        {
            fprintf(stderr, "VECTORIZER: Adding '%s' function version "\
                    "(device=%s, vector_length=%u, target_type=%s, SVML=%d,"\
                    " masked=%d priority=%d)\n",
                    func_name.c_str(), device.c_str(), vector_length,
                    target_type.get_simple_declaration(TL::Scope::get_global_scope(), "").c_str(),
                    masked, is_svml,
                    priority);
        }

        _function_versioning.add_version(func_name,
                VectorFunctionVersion(func_version, device, vector_length, target_type,
                    masked, priority, is_svml));
    }

    bool Vectorizer::is_svml_function(const std::string& func_name,
            const std::string& device,
            const unsigned int vector_length,
            const TL::Type& target_type,
            const bool masked) const
    {
        return _function_versioning.is_svml_function(func_name,
                device, vector_length, target_type, masked);
    }

    void Vectorizer::enable_svml_sse()
    {
        fprintf(stderr, "Enabling SVML SSE\n");

        if (!_svml_sse_enabled)
        {
            _svml_sse_enabled = true;

            // SVML SSE
            TL::Source svml_sse_vector_math;

            svml_sse_vector_math << "__m128 _mm_exp_ps(__m128);\n"
                << "__m128 _mm_sqrt_ps(__m128);\n"
                << "__m128 _mm_log_ps(__m128);\n"
                << "__m128 _mm_sin_ps(__m128);\n"
                << "__m128 _mm_cos_ps(__m128);\n"
                << "__m128 _mm_sincos_ps(__m128*, __m128);\n"
                << "__m128 _mm_floor_ps(__m128);\n"
                << "__m128d _mm_exp_pd(__m128d);\n"
                << "__m128d _mm_sqrt_pd(__m128d);\n"
                << "__m128d _mm_log_pd(__m128d);\n"
                << "__m128d _mm_sin_pd(__m128d);\n"
                << "__m128d _mm_cos_pd(__m128d);\n"
                << "__m128 _mm_sincos_pd(__m128d*, __m128d);\n"
                << "__m128d _mm_floor_pd(__m128d);\n"
                ;

            // Parse SVML declarations
            TL::Scope global_scope = TL::Scope::get_global_scope();
            svml_sse_vector_math.parse_global(global_scope);

            // Add SVML math function as vector version of the scalar one
            add_vector_function_version("expf",
                    global_scope.get_symbol_from_name("_mm_exp_ps").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrtf",
                    global_scope.get_symbol_from_name("_mm_sqrt_ps").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("logf",
                    global_scope.get_symbol_from_name("_mm_log_ps").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sinf",
                    global_scope.get_symbol_from_name("_mm_sin_ps").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sincosf",
                    global_scope.get_symbol_from_name("_mm_sincos_ps").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floorf",
                    global_scope.get_symbol_from_name("_mm_floor_ps").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);

            add_vector_function_version("exp",
                    global_scope.get_symbol_from_name("_mm_exp_pd").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrt",
                    global_scope.get_symbol_from_name("_mm_sqrt_pd").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("log",
                    global_scope.get_symbol_from_name("_mm_log_pd").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sin",
                    global_scope.get_symbol_from_name("_mm_sin_pd").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sincos",
                    global_scope.get_symbol_from_name("_mm_sincos_pd").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floor",
                    global_scope.get_symbol_from_name("_mm_floor_pd").make_nodecl(true),
                    "smp", 16, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
        }
    }

    void Vectorizer::enable_svml_avx2()
    {
        fprintf(stderr, "Enabling SVML AVX2\n");

        if (!_svml_avx2_enabled)
        {
            _svml_avx2_enabled = true;

            // SVML AVX2
            TL::Source svml_avx2_vector_math;

            svml_avx2_vector_math << "__m256 _mm256_exp_ps(__m256);\n"
                << "__m256 _mm256_sqrt_ps(__m256);\n"
                << "__m256 _mm256_log_ps(__m256);\n"
                << "__m256 _mm256_sin_ps(__m256);\n"
                << "__m256 _mm256_cos_ps(__m256);\n"
                << "__m256 _mm256_sincos_ps(__m256*, __m256);\n"
                << "__m256 _mm256_floor_ps(__m256);\n"
                << "__m256d _mm256_exp_pd(__m256d);\n"
                << "__m256d _mm256_sqrt_pd(__m256d);\n"
                << "__m256d _mm256_log_pd(__m256d);\n"
                << "__m256d _mm256_sin_pd(__m256d);\n"
                << "__m256d _mm256_cos_pd(__m256d);\n"
                << "__m256 _mm256_sincos_pd(__m256d*, __m256d);\n"
                << "__m256d _mm256_floor_pd(__m256d);\n"
                ;

            // Parse SVML declarations
            TL::Scope global_scope = TL::Scope::get_global_scope();
            svml_avx2_vector_math.parse_global(global_scope);

            // Add SVML math function as vector version of the scalar one
            add_vector_function_version("expf",
                    global_scope.get_symbol_from_name("_mm256_exp_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrtf",
                    global_scope.get_symbol_from_name("_mm256_sqrt_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("logf",
                    global_scope.get_symbol_from_name("_mm256_log_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sinf",
                    global_scope.get_symbol_from_name("_mm256_sin_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("cosf",
                    global_scope.get_symbol_from_name("_mm256_cos_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sincosf",
                    global_scope.get_symbol_from_name("_mm256_sincos_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floorf",
                    global_scope.get_symbol_from_name("_mm256_floor_ps").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);

            add_vector_function_version("exp",
                    global_scope.get_symbol_from_name("_mm256_exp_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrt",
                    global_scope.get_symbol_from_name("_mm256_sqrt_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("log",
                    global_scope.get_symbol_from_name("_mm256_log_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sin",
                    global_scope.get_symbol_from_name("_mm256_sin_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("cos",
                    global_scope.get_symbol_from_name("_mm256_cos_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sincos",
                    global_scope.get_symbol_from_name("_mm256_sincos_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floor",
                    global_scope.get_symbol_from_name("_mm256_floor_pd").make_nodecl(true),
                    "avx2", 32, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
        }
    }

    void Vectorizer::enable_svml_knc()
    {
        fprintf(stderr, "Enabling SVML KNC\n");

        if (!_svml_knc_enabled)
        {
            _svml_knc_enabled = true;

            // SVML KNC
            TL::Source svml_knc_vector_math;

            // No mask
            svml_knc_vector_math << "__m512 _mm512_exp_ps(__m512);\n"
                << "__m512 _mm512_sqrt_ps(__m512);\n"
                << "__m512 _mm512_log_ps(__m512);\n"
                << "__m512 _mm512_sin_ps(__m512);\n"
                << "__m512 _mm512_cos_ps(__m512);\n"
                //                    << "__m512 _mm512_sincos_ps(__m512*, __m512);\n"
                //                    << "__m512 __svml_sincosf16_ha(__m512*, __m512);\n"
                << "__m512 _mm512_floor_ps(__m512);\n"
                << "__m512d _mm512_exp_pd(__m512d);\n"
                << "__m512d _mm512_sqrt_pd(__m512d);\n"
                << "__m512d _mm512_log_pd(__m512d);\n"
                << "__m512d _mm512_sin_pd(__m512d);\n"
                << "__m512d _mm512_cos_pd(__m512d);\n"
                //                    << "__m512d _mm512_sincos_pd(__m512d, __m512d*);\n"
                << "__m512d _mm512_floor_pd(__m512d);\n"
                ;

            // Mask
            svml_knc_vector_math << "__m512 _mm512_mask_exp_ps(__m512, __mmask16, __m512);\n"
                << "__m512 _mm512_mask_sqrt_ps(__m512, __mmask16, __m512);\n"
                << "__m512 _mm512_mask_log_ps(__m512, __mmask16, __m512);\n"
                << "__m512 _mm512_mask_sin_ps(__m512, __mmask16, __m512);\n"
                << "__m512 _mm512_mask_cos_ps(__m512, __mmask16, __m512);\n"
                //                    << "__m512 _mm512_mask_sincos_ps(__m512, __mmask16, __m512*, __m512);\n"
                //                    << "__m512 __svml_sincosf16_ha_mask(__m512*, __mmask16, __m512);\n"
                << "__m512 _mm512_mask_floor_ps(__m512, __mmask16, __m512);\n"
                << "__m512d _mm512_mask_exp_pd(__m512d, __mmask8, __m512d);\n"
                << "__m512d _mm512_mask_sqrt_pd(__m512d, __mmask8, __m512d);\n"
                << "__m512d _mm512_mask_log_pd(__m512d, __mmask8, __m512d);\n"
                << "__m512d _mm512_mask_sin_pd(__m512d, __mmask8, __m512d);\n"
                << "__m512d _mm512_mask_cos_pd(__m512d, __mmask8, __m512d);\n"
                //                    << "__m512d _mm512_mask_sincos_pd(__m512d, __mmask8, __m512d*);\n"
                << "__m512d _mm512_mask_floor_pd(__m512d, __mmask8, __m512d);\n"
                ;

            // Parse SVML declarations
            TL::Scope global_scope = TL::Scope::get_global_scope();
            svml_knc_vector_math.parse_global(global_scope);

            // Add SVML math function as vector version of the scalar one
            add_vector_function_version("expf",
                    global_scope.get_symbol_from_name("_mm512_exp_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrtf",
                    global_scope.get_symbol_from_name("_mm512_sqrt_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("logf",
                    global_scope.get_symbol_from_name("_mm512_log_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sinf",
                    global_scope.get_symbol_from_name("_mm512_sin_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("cosf",
                    global_scope.get_symbol_from_name("_mm512_cos_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            //It seems it doesn't exist in MIC
            //                add_vector_function_version("sincosf",
            //                            global_scope.get_symbol_from_name("_mm512_sincos_ps").make_nodecl(true),
            //                            "knc", 64, TL::Type::get_float_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floor",
                    global_scope.get_symbol_from_name("_mm512_floor_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("exp",
                    global_scope.get_symbol_from_name("_mm512_exp_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrt",
                    global_scope.get_symbol_from_name("_mm512_sqrt_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("log",
                    global_scope.get_symbol_from_name("_mm512_log_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sin",
                    global_scope.get_symbol_from_name("_mm512_sin_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("cos",
                    global_scope.get_symbol_from_name("_mm512_cos_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            //It seems it doesn't exist in MIC
            //                add_vector_function_version("sincos",
            //                            global_scope.get_symbol_from_name("_mm512_sincos_pd").make_nodecl(true),
            //                            "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floor",
                    global_scope.get_symbol_from_name("_mm512_floor_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), false, DEFAULT_FUNC_PRIORITY, true);


            // Add SVML math masked function as vector version of the scalar one
            add_vector_function_version("expf",
                    global_scope.get_symbol_from_name("_mm512_mask_exp_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrtf",
                    global_scope.get_symbol_from_name("_mm512_mask_sqrt_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("logf",
                    global_scope.get_symbol_from_name("_mm512_mask_log_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sinf",
                    global_scope.get_symbol_from_name("_mm512_mask_sin_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("cosf",
                    global_scope.get_symbol_from_name("_mm512_mask_cos_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            // It seems it doesn't exist in MIC
            //                add_vector_function_version("sincosf",
            //                            global_scope.get_symbol_from_name("_mm512_mask_sincos_ps").make_nodecl(true),
            //                            "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floorf",
                    global_scope.get_symbol_from_name("_mm512_mask_floor_ps").make_nodecl(true),
                    "knc", 64, TL::Type::get_float_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("exp",
                    global_scope.get_symbol_from_name("_mm512_mask_exp_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sqrt",
                    global_scope.get_symbol_from_name("_mm512_mask_sqrt_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("log",
                    global_scope.get_symbol_from_name("_mm512_mask_log_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("sin",
                    global_scope.get_symbol_from_name("_mm512_mask_sin_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("cos",
                    global_scope.get_symbol_from_name("_mm512_mask_cos_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
            // It seems it doesn't exist in MIC
            //                add_vector_function_version("sincos",
            //                            global_scope.get_symbol_from_name("_mm512_mask_sincos_pd").make_nodecl(true),
            //                            "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
            add_vector_function_version("floor",
                    global_scope.get_symbol_from_name("_mm512_mask_floor_pd").make_nodecl(true),
                    "knc", 64, TL::Type::get_double_type(), true, DEFAULT_FUNC_PRIORITY, true);
        }
    }


    void Vectorizer::enable_fast_math()
    {
        _fast_math_enabled = true;
    }
}
}