#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "cxx-cexpr.h"
#include "cxx-ast.h"
#include "cxx-utils.h"
#include "cxx-prettyprint.h"

/*
 * This file implements an evaluator of constant expressions in C++
 */
static void gather_integer_literal_suffix(char* text, char* is_long, char* is_unsigned);
static void promote_values(literal_value_t v1, literal_value_t v2, 
        literal_value_t* out_v1, literal_value_t* out_v2);
static literal_value_t evaluate_conditional_expression(AST condition, 
        AST value_if_true, AST value_if_false, symtab_t* st);
static literal_value_t cast_expression(AST type_spec, AST expression, symtab_t* st);
static literal_value_t binary_operation(node_t op, AST lhs, AST rhs, symtab_t* st);
static literal_value_t evaluate_symbol(AST symbol, symtab_t* st);
static literal_value_t create_value_from_literal(AST a);


typedef literal_value_t (*binary_op_fun)(literal_value_t e1, literal_value_t e2);
typedef struct 
{
    binary_op_fun fun;
    char is_non_strict;
    char if_lhs_zero_eval_rhs;
} binary_operation_t;

#define BINARY_FUNCTION(name) static literal_value_t name(literal_value_t, literal_value_t)
#define BINARY_STRICT(node, fun) [node] = {fun, 0, 0}
#define BINARY_NONSTRICT_IF_LHS_ZERO(node, fun) [node] = {fun, 1, 1}
#define BINARY_NONSTRICT_IF_LHS_NONZERO(node, fun) [node] = {fun, 1, 0}


BINARY_FUNCTION(logical_or);
BINARY_FUNCTION(logical_and);
BINARY_FUNCTION(bitwise_and);
BINARY_FUNCTION(bitwise_or);
BINARY_FUNCTION(bitwise_xor);
BINARY_FUNCTION(different_op);
BINARY_FUNCTION(equal_op);
BINARY_FUNCTION(lower_than);
BINARY_FUNCTION(greater_than);
BINARY_FUNCTION(greater_or_equal_than);
BINARY_FUNCTION(lower_or_equal_than);
BINARY_FUNCTION(shift_right);
BINARY_FUNCTION(shift_left);
BINARY_FUNCTION(addition);
BINARY_FUNCTION(substraction);
BINARY_FUNCTION(division);
BINARY_FUNCTION(multiplication);
BINARY_FUNCTION(module);

literal_value_t not_operation(AST a, symtab_t* st);
literal_value_t negate_operation(AST a, symtab_t* st);
literal_value_t complement_operation(AST a, symtab_t* st);

binary_operation_t binary_ops[] =
{
    BINARY_NONSTRICT_IF_LHS_NONZERO(AST_LOGICAL_OR, logical_or),
    BINARY_NONSTRICT_IF_LHS_ZERO(AST_LOGICAL_AND, logical_and),
    BINARY_STRICT(AST_BITWISE_OR, bitwise_or),
    BINARY_STRICT(AST_BITWISE_XOR, bitwise_xor),
    BINARY_STRICT(AST_BITWISE_AND, bitwise_and),
    BINARY_STRICT(AST_DIFFERENT_OP, different_op),
    BINARY_STRICT(AST_EQUAL_OP, equal_op),
    BINARY_STRICT(AST_LOWER_THAN, lower_than),
    BINARY_STRICT(AST_GREATER_THAN, greater_than),
    BINARY_STRICT(AST_GREATER_OR_EQUAL_THAN, greater_or_equal_than),
    BINARY_STRICT(AST_LOWER_OR_EQUAL_THAN, lower_or_equal_than),
	BINARY_STRICT(AST_SHL_OP, shift_left),
	BINARY_STRICT(AST_SHR_OP, shift_right),
	BINARY_STRICT(AST_ADD_OP, addition),
	BINARY_STRICT(AST_MINUS_OP, substraction),
	BINARY_STRICT(AST_MULT_OP, multiplication),
	BINARY_STRICT(AST_MOD_OP, module),
	BINARY_STRICT(AST_DIV_OP, division),
};


literal_value_t evaluate_constant_expression(AST a, symtab_t* st)
{
    switch (ASTType(a))
	{
		case AST_CONSTANT_EXPRESSION :
		case AST_PARENTHESIZED_EXPRESSION :
			return evaluate_constant_expression(ASTSon0(a), st);
		case AST_QUALIFIED_ID :
		case AST_SYMBOL :
			return evaluate_symbol(a, st);
			break;
		case AST_LOGICAL_OR :
		case AST_LOGICAL_AND :
		case AST_BITWISE_OR :
		case AST_BITWISE_XOR :
		case AST_BITWISE_AND :
		case AST_DIFFERENT_OP :
		case AST_EQUAL_OP :
		case AST_LOWER_THAN :
		case AST_GREATER_THAN :
		case AST_GREATER_OR_EQUAL_THAN :
		case AST_LOWER_OR_EQUAL_THAN :
		case AST_SHL_OP :
		case AST_SHR_OP :
		case AST_ADD_OP :
		case AST_MINUS_OP :
		case AST_MULT_OP :
		case AST_MOD_OP :
		case AST_DIV_OP :
			return binary_operation(ASTType(a), ASTSon0(a), ASTSon1(a), st);
		case AST_CAST_EXPRESSION :
			return cast_expression(ASTSon0(a), ASTSon1(a), st);
		case AST_CONDITIONAL_EXPRESSION :
			return evaluate_conditional_expression(ASTSon0(a), ASTSon1(a), ASTSon2(a), st);
		case AST_DECIMAL_LITERAL :
		case AST_OCTAL_LITERAL :
		case AST_HEXADECIMAL_LITERAL :
		case AST_CHARACTER_LITERAL :
		case AST_BOOLEAN_LITERAL :
			return create_value_from_literal(a);
		case AST_PLUS_OP :
			return evaluate_constant_expression(ASTSon0(a), st);
		case AST_NOT_OP :
			return not_operation(ASTSon0(a), st);
		case AST_NEG_OP :
			return negate_operation(ASTSon0(a), st);
		case AST_COMPLEMENT_OP :
			return complement_operation(ASTSon0(a), st);
		default :
			internal_error("Unsupported node '%s' when evaluating constant expression", ast_print_node_type(ASTType(a)));
	}
}


static literal_value_t binary_operation(node_t op, AST lhs, AST rhs, symtab_t* st)
{
    literal_value_t val_lhs;
    literal_value_t val_rhs;
    binary_operation_t bop;

    val_lhs = evaluate_constant_expression(lhs, st);

    bop = binary_ops[op];

    if (bop.is_non_strict)
    {
        if (bop.if_lhs_zero_eval_rhs)
        {
            if (value_is_zero(val_lhs))
            {
                val_rhs = evaluate_constant_expression(rhs, st);
            }
        }
        else
        {
            if (!value_is_zero(val_lhs))
            {
                val_rhs = evaluate_constant_expression(rhs, st);
            }
        }
    }
    else
    {
        val_rhs = evaluate_constant_expression(rhs, st);
    }

    promote_values(val_lhs, val_rhs, &val_lhs, &val_rhs);

    if (val_lhs.kind != val_rhs.kind)
    {
        internal_error("Both types should be the same (%d != %d)", val_lhs.kind, val_rhs.kind);
    }

    return bop.fun(val_lhs, val_rhs);
}


static literal_value_t evaluate_conditional_expression(AST condition, AST value_if_true, AST value_if_false, symtab_t* st)
{
    literal_value_t condition_value = evaluate_constant_expression(condition, st);

    if (value_is_zero(condition_value))
    {
        return evaluate_constant_expression(value_if_false, st);
    }
    else
    {
        return evaluate_constant_expression(value_if_true, st);
    }
}

char value_is_zero(literal_value_t v)
{
    switch (v.kind)
    {
        case LVK_UNSIGNED_LONG :
            return (v.value.unsigned_long == 0);
        case LVK_UNSIGNED_INT :
            return (v.value.unsigned_int == 0);
        case LVK_SIGNED_LONG :
            return (v.value.signed_long == 0);
        case LVK_SIGNED_INT :
            return (v.value.signed_int == 0);
        case LVK_CHARACTER :
            return (v.value.character_value == 0);
        case LVK_BOOL :
            return (v.value.boolean_value == 0);
        default:
            internal_error("Invalid value kind %d\n", v.kind);
    }
}

static literal_value_t create_value_from_literal(AST a)
{
    literal_value_t result;
    memset(&result, 0, sizeof(result));

    char is_long, is_unsigned;
    char* literal_text = ASTText(a);
    // literal rule
    switch (ASTType(a))
    {
        case AST_DECIMAL_LITERAL :
        case AST_OCTAL_LITERAL :
        case AST_HEXADECIMAL_LITERAL :
        case AST_FLOATING_LITERAL :
            gather_integer_literal_suffix(literal_text, &is_long, &is_unsigned);
            if (is_long) 
            {
                if (is_unsigned)
                {
                    result.kind = LVK_UNSIGNED_LONG;
                    result.value.unsigned_long = strtoul(literal_text, NULL, 0);
                }
                else
                {
                    result.kind = LVK_SIGNED_LONG;
                    result.value.signed_long = strtol(literal_text, NULL, 0);
                }
            }
            else // Is not long
            {
                // We would use strtod if it supported octals
                if (is_unsigned)
                {
                    result.kind = LVK_UNSIGNED_INT;
                    result.value.unsigned_int = strtol(literal_text, NULL, 0);
                }
                else 
                {
                    result.kind = LVK_SIGNED_INT;
                    result.value.signed_int = strtoul(literal_text, NULL, 0);
                }
            }
            break;
        case AST_BOOLEAN_LITERAL : 
            result.kind = LVK_BOOL;
            result.value.boolean_value = (strcmp(literal_text, "false")) ? 0 : 1;
            break;
        case AST_CHARACTER_LITERAL :
            if (toupper(literal_text[0]) == 'L')
            {
                // We should use iconv or something similar to get this correct
                internal_error("Unsupported wide character in constant expression evaluation", 0);
            }
            else
            {
                result.kind = LVK_CHARACTER;
                if (literal_text[1] != '\\')
                {
                    result.value.character_value = literal_text[1];
                }
                else
                {
                    internal_error("TODO - Check for escape sentences!", 0);
                }
            }
            break;
        default :
            internal_error("Unknown literal", 0);
            break;
    }

    return result;
}

static void gather_integer_literal_suffix(char* text, char* is_long, char* is_unsigned)
{
    int i = 0;
    char* suffix = &text[strlen(text) - 1];

    *is_long = 0;
    *is_unsigned = 0;

    while ((i < 3)
            && (toupper(*suffix) == 'U' 
                || toupper(*suffix) == 'L'))
    {
        if (toupper(*suffix) == 'L')
        {
            (*is_long)++;
        }
        else // if (toupper(*suffix) == 'U')
        {
            *is_unsigned = 1;
        }
        suffix--;
        i++;
    }
}

// This implements "usual arithmetic conversions" as defined in clause 5 paragraph 9
// of C++ standard
static void promote_values(literal_value_t v1, literal_value_t v2, 
        literal_value_t* out_v1, literal_value_t* out_v2)
{
    *out_v1 = v1;
    *out_v2 = v2;
    if (v1.kind == v2.kind)
    {
        return;
    }

    literal_value_t* in_val[] = {&v1, &v2, NULL};
    literal_value_t* out_val[] = {out_v1, out_v2, NULL};

    // Float and double are not considered
    // Directly apply integral promotions
    int i;
    for (i = 0; i < 2; i++)
    {
        literal_value_t* in = in_val[i];
        literal_value_t* out = out_val[i];

        if (in->kind == LVK_CHARACTER)
        {
            out->kind = LVK_SIGNED_INT;
            out->value.unsigned_int = (unsigned int) in->value.character_value;
        }

        if (in->kind == LVK_BOOL)
        {
            out->kind = LVK_SIGNED_INT;
            out->value.unsigned_int = (unsigned int) in->value.boolean_value;
        }
    }

    // After this everything is either a LVK_*_LONG or LVK_*_INT

    // if either one operand is "unsigned long" convert the other to "unsigned
    // long"
    if ((v1.kind == LVK_UNSIGNED_LONG)
            ^ (v2.kind == LVK_UNSIGNED_LONG))
    {
        if (v1.kind == LVK_UNSIGNED_LONG)
        {
            out_v2->kind = LVK_UNSIGNED_LONG;
            switch (v2.kind)
            {
                case LVK_SIGNED_LONG :
                    out_v2->value.unsigned_long = (unsigned long) v2.value.signed_long;
                    break;
                case LVK_SIGNED_INT :
                    out_v2->value.unsigned_long = (unsigned long) v2.value.signed_int;
                    break;
                case LVK_UNSIGNED_INT :
                    out_v2->value.unsigned_long = (unsigned long) v2.value.unsigned_int;
                    break;
                default :
                    internal_error("Unknown literal value type", v2.kind);
            }
        }
        else
        {
            out_v1->kind = LVK_UNSIGNED_LONG;
            switch (v1.kind)
            {
                case LVK_SIGNED_LONG :
                    out_v1->value.unsigned_long = (unsigned long) v1.value.signed_long;
                    break;
                case LVK_SIGNED_INT :
                    out_v1->value.unsigned_long = (unsigned long) v1.value.signed_int;
                    break;
                case LVK_UNSIGNED_INT :
                    out_v1->value.unsigned_long = (unsigned long) v1.value.unsigned_int;
                    break;
                default :
                    internal_error("Unknown literal value type", v1.kind);
            }
        }
    }

    // If one operand is "signed long" and the other "unsigned int" convert to
    // "signed long" the "unsigned int" if a "signed long" can represent every
    // value of a "signed long", otherwise convert the "signed long" to a
    // "unsigned long"
    if ((v1.kind == LVK_SIGNED_LONG && v2.kind == LVK_UNSIGNED_INT)
            || (v2.kind == LVK_SIGNED_LONG && v1.kind == LVK_UNSIGNED_INT))
    {
        // Assume that a long int can hold any unsigned int. Otherwise
        // they should be converted to unsigned long int
        if (v1.kind == LVK_SIGNED_LONG 
                && v2.kind == LVK_UNSIGNED_INT)
        {
            out_v2->kind = LVK_SIGNED_LONG;
            out_v2->value.signed_long = (signed long) v2.value.unsigned_int;
        }
        else
        {
            out_v1->kind = LVK_SIGNED_LONG;
            out_v1->value.signed_long = (signed long) v1.value.unsigned_int;
        }
    }

    // either one operand is long int
    if ((v1.kind == LVK_SIGNED_LONG)
            ^ (v2.kind == LVK_SIGNED_LONG))
    {
        if (v1.kind == LVK_SIGNED_LONG)
        {
            out_v2->kind = LVK_SIGNED_LONG;
            switch (v2.kind)
            {
                case LVK_UNSIGNED_LONG :
                    out_v2->value.signed_long = (signed long) v2.value.unsigned_long;
                    break;
                case LVK_SIGNED_INT :
                    out_v2->value.signed_long = (signed long) v2.value.signed_int;
                    break;
                case LVK_UNSIGNED_INT :
                    out_v2->value.signed_long = (signed long) v2.value.unsigned_int;
                    break;
                default :
                    internal_error("Unknown literal value type", v2.kind);
            }
        }
        else
        {
            out_v1->kind = LVK_SIGNED_LONG;
            switch (v1.kind)
            {
                case LVK_UNSIGNED_LONG :
                    out_v1->value.signed_long = (signed long) v1.value.unsigned_long;
                    break;
                case LVK_SIGNED_INT :
                    out_v1->value.signed_long = (signed long) v1.value.signed_int;
                    break;
                case LVK_UNSIGNED_INT :
                    out_v1->value.signed_long = (signed long) v1.value.unsigned_int;
                    break;
                default :
                    internal_error("Unknown literal value type", v1.kind);
            }
        }
    }

    // either one operand is "unsigned" the other shall be converted to "unsigned"
    if ((v1.kind == LVK_UNSIGNED_LONG 
                && v2.kind == LVK_SIGNED_LONG)
            || (v1.kind == LVK_SIGNED_LONG
                && v2.kind == LVK_UNSIGNED_LONG))
    {
        if (v1.kind == LVK_SIGNED_LONG)
        {
            out_v1->kind = LVK_UNSIGNED_LONG;
            out_v1->value.unsigned_long = (unsigned long) v1.value.signed_long;
        }
        else
        {
            out_v2->kind = LVK_UNSIGNED_LONG;
            out_v2->value.unsigned_long = (unsigned long) v2.value.signed_long;
        }
    }
    if ((v1.kind == LVK_UNSIGNED_INT 
                && v2.kind == LVK_SIGNED_INT)
            || (v1.kind == LVK_SIGNED_INT
                && v2.kind == LVK_UNSIGNED_INT))
    {
        if (v1.kind == LVK_SIGNED_INT)
        {
            out_v1->kind = LVK_UNSIGNED_INT;
            out_v1->value.unsigned_int = (unsigned int) v1.value.signed_int;
        }
        else
        {
            out_v2->kind = LVK_UNSIGNED_INT;
            out_v2->value.unsigned_int = (unsigned int) v2.value.signed_int;
        }
    }
}

static literal_value_t cast_expression(AST type_spec, AST expression, symtab_t* st)
{
#warning TODO This has to cast !
	return evaluate_constant_expression(expression, st);
}

static literal_value_t evaluate_symbol(AST symbol, symtab_t* st)
{
	symtab_entry_list_t* result = query_id_expression(st, symbol);

	if (result == NULL)
	{
		fprintf(stderr, "Unknown symbol '");
		prettyprint(stderr, symbol);
		fprintf(stderr, "'");
		internal_error("Cannot evaluate unknown symbol", 0);
	}

	if (result->entry->kind != SK_ENUMERATOR
			&& result->entry->kind != SK_VARIABLE)
	{
		fprintf(stderr, "Invalid symbol '");
		prettyprint(stderr, symbol);
		fprintf(stderr, "'");
		internal_error("This symbol is not an expression", 0);
	}

	if (result->entry->expression_value == NULL)
	{
		fprintf(stderr, "Invalid symbol '");
		prettyprint(stderr, symbol);
		fprintf(stderr, "'");
		internal_error("This symbol does not have a value", 0);
	}

	return evaluate_constant_expression(result->entry->expression_value, st);
}

literal_value_t literal_value_zero()
{
	literal_value_t result;

	result.kind = LVK_SIGNED_INT;
	result.value.signed_int = 0;

	return result;
}

literal_value_t literal_value_minus_one()
{
	literal_value_t result;

	result.kind = LVK_SIGNED_INT;
	result.value.signed_int = -1;

	return result;
}

literal_value_t increment_literal_value(literal_value_t e)
{
	literal_value_t result = e;
	switch (result.kind)
	{
		case LVK_SIGNED_INT :
			result.value.signed_int++;
			break;
		case LVK_UNSIGNED_INT :
			result.value.unsigned_int++;
			break;
		case LVK_SIGNED_LONG :
			result.value.signed_long++;
			break;
		case LVK_UNSIGNED_LONG :
			result.value.unsigned_long++;
			break;
		case LVK_CHARACTER :
			result.value.character_value++;
			break;
		default :
			internal_error("This is not a valid value to be incremented", 0);
			break;
	}

	return result;
}

AST tree_from_literal_value(literal_value_t e)
{
	char buffer[100];
	AST result;

	buffer[99] = '\0';

	switch (e.kind)
	{
		case LVK_SIGNED_INT :
			snprintf(buffer, 99, "%d", e.value.signed_int);
			result = ASTLeaf(AST_DECIMAL_LITERAL, 0, buffer);
			break;
		case LVK_SIGNED_LONG :
			snprintf(buffer, 99, "%ldL", e.value.signed_long);
			result = ASTLeaf(AST_DECIMAL_LITERAL, 0, buffer);
			break;
		case LVK_UNSIGNED_INT :
			snprintf(buffer, 99, "%uU", e.value.unsigned_int);
			result = ASTLeaf(AST_DECIMAL_LITERAL, 0, buffer);
			break;
		case LVK_UNSIGNED_LONG :
			snprintf(buffer, 99, "%luLU", e.value.unsigned_long);
			result = ASTLeaf(AST_DECIMAL_LITERAL, 0, buffer);
			break;
		case LVK_CHARACTER :
			snprintf(buffer, 99, "'%c'", e.value.character_value);
			result = ASTLeaf(AST_CHARACTER_LITERAL, 0, buffer);
			break;
		case LVK_BOOL :
			if (e.value.boolean_value == 0)
			{
				result = ASTLeaf(AST_BOOLEAN_LITERAL, 0, "false");
			}
			else
			{
				result = ASTLeaf(AST_BOOLEAN_LITERAL, 0, "true");
			}
			break;
		default:
			result = NULL;
			internal_error("Invalid type kind %d\n", e.kind);
	}

	return result;
}

/* ************************* *
 *    Unary operations       *
 * ************************* */


literal_value_t not_operation(AST a, symtab_t* st)
{
    literal_value_t result = evaluate_constant_expression(a, st);
    switch (result.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = ! result.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = ! result.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = ! result.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = ! result.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = ! result.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = ! result.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", result.kind);
    }

    return result;
}

literal_value_t negate_operation(AST a, symtab_t* st)
{
    literal_value_t result = evaluate_constant_expression(a, st);
    switch (result.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = - result.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = - result.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = - result.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = - result.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = - result.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = - result.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", result.kind);
    }

    return result;
}

literal_value_t complement_operation(AST a, symtab_t* st)
{
    literal_value_t result = evaluate_constant_expression(a, st);
    switch (result.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = ~ result.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = ~ result.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = ~ result.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = ~ result.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = ~ result.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = ~ result.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", result.kind);
    }

    return result;
}


/* ************************* *
 *    Binary operations      *
 * ************************* */

static literal_value_t logical_and(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
	result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long && e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long && e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int && e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int && e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value && e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value && e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t bitwise_and(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long & e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long & e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int & e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int & e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value & e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value & e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t logical_or(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
	result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long || e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long || e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int || e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int || e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value || e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value || e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t bitwise_or(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long | e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long | e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int | e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int | e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value | e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value | e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t bitwise_xor(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long ^ e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long ^ e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int ^ e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int ^ e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value ^ e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value ^ e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t different_op(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long != e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long != e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int != e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int != e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value != e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value != e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t equal_op(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long == e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long == e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int == e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int == e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value == e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value == e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t lower_than(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long < e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long < e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int < e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int < e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value < e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value < e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t greater_than(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long > e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long > e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int > e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int > e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value > e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value > e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t lower_or_equal_than(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long <= e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long <= e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int <= e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int <= e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value <= e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value <= e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t greater_or_equal_than(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    result.kind = LVK_BOOL;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.boolean_value = e1.value.unsigned_long >= e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.boolean_value = e1.value.signed_long >= e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.boolean_value = e1.value.unsigned_int >= e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.boolean_value = e1.value.signed_int >= e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.boolean_value = e1.value.character_value >= e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value >= e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t shift_left(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long << e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long << e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int << e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int << e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value << e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value << e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t shift_right(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long >> e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long >> e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int >> e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int >> e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value >> e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value >> e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t addition(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long + e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long + e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int + e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int + e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value + e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value + e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t substraction(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long - e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long - e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int - e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int - e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value - e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value - e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t multiplication(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long * e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long * e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int * e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int * e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value * e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value * e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t division(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long / e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long / e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int / e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int / e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value / e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value / e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

static literal_value_t module(literal_value_t e1, literal_value_t e2)
{
    literal_value_t result = e1;
    switch (e1.kind)
    {
        case LVK_UNSIGNED_LONG :
            result.value.unsigned_long = e1.value.unsigned_long % e2.value.unsigned_long;
            break;
        case LVK_SIGNED_LONG :
            result.value.signed_long = e1.value.signed_long % e2.value.signed_long;
            break;
        case LVK_UNSIGNED_INT :
            result.value.unsigned_int = e1.value.unsigned_int % e2.value.unsigned_int;
            break;
        case LVK_SIGNED_INT :
            result.value.signed_int = e1.value.signed_int % e2.value.signed_int;
            break;
        case LVK_CHARACTER :
            result.value.character_value = e1.value.character_value % e2.value.character_value;
            break;
        case LVK_BOOL :
            result.value.boolean_value = e1.value.boolean_value % e2.value.boolean_value;
            break;
        default:
            internal_error("Unknown value kind %d", e1.kind);
    }

    return result;
}

