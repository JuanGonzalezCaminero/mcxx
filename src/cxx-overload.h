#ifndef CXX_OVERLOAD_H
#define CXX_OVERLOAD_H

#include "cxx-ast.h"
#include "cxx-scope.h"

enum ics_kind
{
	ICS_UNKNOWN = 0,
	ICS_STANDARD,
	ICS_USER_DEFINED,
	ICS_ELLIPSIS,
};

#define BITMAP(x) (1 << x)

enum scs_category_t
{
	SCS_UNKNOWN = 0,
	SCS_IDENTITY = BITMAP(1),
	SCS_LVALUE_TRANSFORMATION = BITMAP(2),
	SCS_PROMOTION = BITMAP(3),
	SCS_CONVERSION = BITMAP(4),
	SCS_QUALIFICATION_ADJUSTMENT = BITMAP(5),
};

#undef BITMAP

typedef struct one_implicit_conversion_sequence_tag
{
	enum ics_kind kind;
	enum scs_category_t scs_category;
} one_implicit_conversion_sequence_t;

// Represents a whole ICS for all arguments
typedef struct implicit_conversion_sequence_tag
{
	int num_arg;
	one_implicit_conversion_sequence_t** conversion;
} implicit_conversion_sequence_t;

typedef struct viable_function_list_tag
{
	scope_entry_t* entry;
	int ics_num_args;
	implicit_conversion_sequence_t* ics;
	struct viable_function_list_tag* next;
} viable_function_list_t;

scope_entry_t* resolve_overload(scope_t* st, AST argument_list, scope_entry_list_t* candidate_functions);

#endif // CXX_OVERLOAD
