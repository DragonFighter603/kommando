#include "infer.h"

#include "ast.h"

#include "lib/str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void drop_variable(Variable var) {
    free(var.name);
    free(var.type);
}

void push_frame(ScopeStack* stack) {
    LocalScope frame = list_new();
    list_append(stack, frame);
}

void drop_loal_scope(LocalScope frame) {
    list_foreach(&frame, drop_variable);
    free(frame.elements);
}

void pop_frame(ScopeStack* stack) {
    drop_loal_scope(list_pop(stack));
}

str get_var_type(ScopeStack* stack, str name) {
    if (strcmp(name, "NULL") == 0) {
        return "&any";
    }
    for (int i = stack->length - 1;i >= 0;i--) {
        for (int j = stack->elements[i].length - 1;j >= 0;j--) {
            if (strcmp(name, stack->elements[i].elements[j].name) == 0) {
                return stack->elements[i].elements[j].type;
            }
        }
    }
    fprintf(stderr, "No such variable %s\n", name);
    exit(1);
}

void assert_type_match(str type1, str type2, usize loc) {
    if (type1[0] == '&' && type2[0] == '&') {
        assert_type_match(type1 + 1, type2 + 1, loc);
        return;
    }
    if (strcmp(type1, "any") == 0) return;
    if (strcmp(type2, "any") == 0) return;
    if (strcmp(type1, type2) != 0) {
        printf("Types `%s` and `%s` do not match (near line %lld)\n", type1, type2, loc + 1);
        exit(1);
    }
}

void register_var(ScopeStack* stack, str name, str type) {
    Variable var = { copy_str(name), copy_str(type) };
    list_append(&(stack->elements[stack->length-1]), var);
}

void infer_field_access(Module* module, ScopeStack* stack, str parent_type, Expression* field) {
    switch (field->kind) {
        case FIELD_ACCESS_EXPR: {
            FieldAccess* fa = field->expr;
            infer_field_access(module, stack, parent_type, fa->object);
            infer_field_access(module, stack, fa->object->type, fa->field);
            field->type = copy_str(fa->field->type);
        } break;
        case VARIABLE_EXPR: {
            str field_name = field->expr;
            Type* t = find_type(module, parent_type, field->src_line);
            deref:
            switch (t->type) {
                case TYPE_PRIMITIVE:
                    fprintf(stderr, "primitive `%s` has no such field `%s` (near line %lld)\n", parent_type, field_name, field->src_line + 1);
                    exit(1);
                case TYPE_STRUCT: {
                    Struct* s = t->ty;
                    for (usize i = 0;i < s->fields.length;i++) {
                        if (strcmp(s->fields.elements[i], field_name) == 0) {
                            field->type = copy_str(s->fields_t.elements[i]);
                            goto found;
                        }
                    }
                    fprintf(stderr, "struct `%s` (`%s`) has no such field `%s` (near line %lld)\n", s->name, parent_type, field_name, field->src_line + 1);
                    exit(1);
                    found: {}
                } break;
                case TYPE_POINTER:
                    t = find_type(module, parent_type + 1, field->src_line);
                    goto deref;
            }
        } break;
        default:
            fprintf(stderr, "unreachable field kind (%d)\n", field->kind);
            exit(1);
    }
}

void __infer_types_expression(Module* module, ScopeStack* stack, Expression* expr) {
    switch (expr->kind) {
        case FUNC_CALL_EXPR: {
            FunctionCall* fc = expr->expr;
            FunctionDef* func = find_func(module, fc->name, expr->src_line);
            if (func->is_variadic) {
                if (func->args.length > fc->args.length) {
                    fprintf(stderr, "called variadic function with %lld args but expected at least %lld (near line %lld)", fc->args.length, func->args.length, expr->src_line);
                    exit(1);
                }
            } else {
                if (func->args.length != fc->args.length) {
                    fprintf(stderr, "called function with %lld args but expected %lld (near line %lld)", fc->args.length, func->args.length, expr->src_line);
                    exit(1);
                }
            }
            for (int i = 0;i < fc->args.length;i++) {
                infer_types_expression(module, stack, fc->args.elements[i]);
                if (i < func->args.length) assert_type_match(fc->args.elements[i]->type, func->args_t.elements[i], fc->args.elements[i]->src_line);
            }
            expr->type = copy_str(func->ret_t);
        } break;
        case BLOCK_EXPR: {
            infer_types_block(module, stack, expr->expr);
            expr->type = copy_str("unit");
        } break;
        case IF_EXPR: {
            IfExpr* if_expr = expr->expr;
            infer_types_expression(module, stack, if_expr->condition);
            assert_type_match(if_expr->condition->type, "bool", expr->src_line);
            infer_types_block(module, stack, if_expr->then);
            if (if_expr->otherwise != NULL) infer_types_block(module, stack, if_expr->otherwise);
            expr->type = copy_str("unit");
        } break;
        case WHILE_EXPR: {
            WhileExpr* while_expr = expr->expr;
            infer_types_expression(module, stack, while_expr->condition);
            assert_type_match(while_expr->condition->type, "bool", expr->src_line);
            infer_types_block(module, stack, while_expr->body);
            expr->type = copy_str("unit");
        } break;
        case LITERAL_EXPR: {
            Token* lit = expr->expr;
            switch (lit->type) {
                case STRING:
                    expr->type = copy_str("&char");
                    break;
                case NUMERAL: {
                    usize len = strlen(lit->string);
                    for (int i = 0;i < len;i++) {
                        if (lit->string[i] == 'u' || lit->string[i] == 'i' || lit->string[i] == 'f' || lit->string[i] == 'b') {
                            expr->type = copy_str(lit->string+i);
                            lit->string[i] = '\0';
                            goto end;
                        }
                    }
                    expr->type = copy_str("i32");
                    end: {}
                } break;
                default:
                    fprintf(stderr, "unreachable case: %s\n", TOKENTYPE_STRING[lit->type]);
                    exit(1);
            }
        } break;
        case VARIABLE_EXPR: {
            str var = expr->expr;
            expr->type = copy_str(get_var_type(stack, var));
        } break;
        case RETURN_EXPR: {
            infer_types_expression(module, stack, expr->expr);
            expr->type = copy_str("unit");
        } break;
        case STRUCT_LITERAL: {
            if (strcmp(expr->expr, "unit") == 0) {
                expr->type = copy_str("unit");
            } else {
                fprintf(stderr, "todo: STRUCT_LITERAL infer (%lld)\n", expr->src_line + 1);
                exit(1);   
            }
        } break;
        case REF_EXPR: {
            infer_types_expression(module, stack, expr->expr);
            str inner = ((Expression*)expr->expr)->type;
            usize len = strlen(inner);
            expr->type = malloc(len + 2);
            expr->type[0] = '&';
            strcpy(expr->type + 1, inner);
            expr->type[len + 1] = '\0';
        } break;
        case DEREF_EXPR: {
            infer_types_expression(module, stack, expr->expr);
            str inner = ((Expression*)expr->expr)->type;
            expr->type = copy_str(inner + 1);
        } break;
        case VAR_DECL_EXPR: {
            VarDecl* vd = expr->expr;
            if (vd->value != NULL) {
                infer_types_expression(module, stack, vd->value);
                assert_type_match(vd->value->type, vd->type, expr->src_line);
            }
            register_var(stack, vd->name, vd->type);
            expr->type = copy_str("unit");
        } break;
        case ASSIGN_EXPR: {
            Assign* ass = expr->expr;
            infer_types_expression(module, stack, ass->target);
            infer_types_expression(module, stack, ass->value);
            assert_type_match(ass->target->type, ass->value->type, expr->src_line);
            expr->type = copy_str("unit");
        } break;
        case FIELD_ACCESS_EXPR: {
            FieldAccess* fa = expr->expr;
            infer_types_expression(module, stack, fa->object);
            infer_field_access(module, stack, fa->object->type, fa->field);
            expr->type = copy_str(fa->field->type);          
        } break;
        case BINOP_EXPR: {
            BinOp* bo = expr->expr;
            infer_types_expression(module, stack, bo->lhs);
            infer_types_expression(module, stack, bo->rhs);
            assert_type_match(bo->lhs->type, bo->rhs->type, expr->src_line);
            if (strcmp(bo->op, "&&") == 0 || strcmp(bo->op, "||") == 0) {
                assert_type_match(bo->lhs->type, "bool", expr->src_line);
                expr->type = copy_str("bool");
            } else if (bo->op[0] == '!' || bo->op[0] == '=' || bo->op[0] == '>' || bo->op[0] == '<') {
                expr->type = copy_str("bool");
            } else {
                expr->type = copy_str(bo->lhs->type);
            } 
        } break;
    }
}

void infer_types_block(Module* module, ScopeStack* stack, Block* block) {
    push_frame(stack);
    for (usize i = 0;i < block->exprs.length;i++) {
        Expression* expr = block->exprs.elements[i];
        infer_types_expression(module, stack, expr);
    }
    pop_frame(stack);
}

void infer_types_fn(Module* module, ScopeStack* stack, FunctionDef* fn) {
    push_frame(stack);
    for (int i = 0;i < fn->args.length;i++) {
        register_var(stack, fn->args.elements[i], fn->args_t.elements[i]);
    }
    infer_types_block(module, stack, fn->body);
    pop_frame(stack);
}

void infer_types(Module* module) {
    ScopeStack stack = list_new();
    for (usize i = 0;i < module->funcs.length;i++) {
        if (module->funcs.elements[i]->body == NULL) continue; 
        infer_types_fn(module, &stack, module->funcs.elements[i]);
        drop_temp_types();
        printf("    inferred types for function `%s`\n", module->funcs.elements[i]->name);
    }
    free(stack.elements);
}