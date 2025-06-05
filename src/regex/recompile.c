//
// Created by manni on 2025/6/5.
//

#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "regex.h"
#include "recompile.h"

static void nfa_set_name(struct NFA *nfa, char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(nfa->name, sizeof(nfa->name), format, args);
    va_end(args);
}

#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))


static struct NFA *make_node(struct RegexInner *restrict inner, RegexErr *err) {
    struct NFA *nfa = (struct NFA *) calloc(1, sizeof(struct NFA));
    if (!nfa) {
        return NULL;
    }
    void *ptr = realloc(inner->nodes, sizeof(struct NFA *) * (inner->node_count + 1));
    if (!ptr) {
        free(nfa);
        // 这里不释放 inner->nodes ，由refree统一释放
        *err = REGEX_NOMEM;
        return NULL;
    }
    inner->nodes = ptr;
    inner->nodes[inner->node_count] = nfa;
    inner->node_count++;
    nfa_set_name(nfa, "n%zu", inner->node_count);
    return nfa;
}

static void free_node(struct NFA *node) {
    // 没有任何意义，主要是为了对称性，当然以后可能会增加自定义的malloc函数，那就有意义了。
    free(node);
}

static RegexErr add_epsilon(struct NFA *nfa, struct NFA *epsilon, RegexErr *err) {
    void *ptr = realloc(nfa->epsilon, sizeof(struct NFA *) * (nfa->epsilon_count + 1));
    if (!ptr) {
        free(nfa->epsilon);
        *err = REGEX_NOMEM;
        return *err;
    }
    nfa->epsilon = ptr;
    nfa->epsilon[nfa->epsilon_count] = epsilon;
    nfa->epsilon_count++;
    return REGEX_OK;
}

static RegexErr add_literal(struct RegexInner *restrict inner, struct Fragment *frag, char ch, RegexErr *err) {
    struct NFA *node = make_node(inner, err);
    if (!node) {
        return *err;
    }
    frag->end->next[(unsigned char) ch] = node;
    frag->start = frag->end;
    frag->end = node;
    return *err;
}

static RegexErr add_dot(struct RegexInner *restrict inner, struct Fragment *frag, RegexErr *err) {
    struct NFA *node = make_node(inner, err);
    if (!node) {
        return *err;
    }
    for (size_t i = 0; i < ARR_LEN(frag->end->next); i++) {
        frag->end->next[i] = node;
    }
    frag->start = frag->end;
    frag->end = node;
    return *err;
}

static RegexErr add_question(struct RegexInner *restrict inner, struct Fragment *frag, RegexErr *err) {
    struct NFA *node = make_node(inner, err);
    if (!node) {
        return *err;
    }
    if (add_epsilon(frag->end, node, err) != REGEX_OK) {
        return *err;
    }
    if (add_epsilon(frag->start, node, err) != REGEX_OK) {
        return *err;
    }
    frag->end = node;
    return *err;
}

static RegexErr add_plus(struct Fragment *frag, RegexErr *err) {
    if (add_epsilon(frag->end, frag->start, err) != REGEX_OK) {
        return *err;
    }
    return *err;
}

static RegexErr add_star(struct Fragment *frag, RegexErr *err) {
    if (add_epsilon(frag->end, frag->start, err) != REGEX_OK) {
        return *err;
    }
    if (add_epsilon(frag->start, frag->end, err) != REGEX_OK) {
        return *err;
    }
    return *err;
}

static RegexErr new_empty_fragment(struct RegexInner *inner, struct Fragment *frag, int group, RegexErr *err) {
    // 只有捕获组才会需要新增一个fragment，其他情况都是直接操作原有的fragment即可
    struct NFA *const start = make_node(inner, err);
    if (!start) {
        return *err;
    }
    struct NFA *const end = make_node(inner, err);
    if (!end) {
        free_node(start);
        return *err;
    }
    frag->start = start;
    frag->end = end;
    nfa_set_name(start, "S(G[%d])", group);
    nfa_set_name(end, "E(G[%d])", group);
    return REGEX_OK;
}

Regex *recompile(char *input, size_t len, RegexErr *err) {
    struct RegexInner *inner = calloc(1, sizeof(struct RegexInner));
    if (!inner) {
        return NULL;
    }
    inner->pattern = strdup(input);
    if (!inner->pattern) {
        refree_inner(inner);
        return NULL;
    }
    struct Fragment *const stack = (struct Fragment *) calloc(len, sizeof(struct Fragment));
    if (!stack) {
        refree_inner(inner);
        return NULL;
    }
    // 制作一个dummy root
    if (new_empty_fragment(inner, &stack[0], 0, err) != REGEX_OK) {
        free(stack);
        refree_inner(inner);
        return NULL;
    }
    inner->start = stack[0].start;
    stack[0].end->accepted = true;

    size_t top = 0;

    struct NFA *first = make_node(inner, err);
    if (!first) {
        free(stack);
        refree_inner(inner);
        return NULL;
    }
    struct Fragment current = {.start = first, .end = first};
    if (add_epsilon(stack[top].start, first, err) != REGEX_OK) {
        free(stack);
        refree_inner(inner);
        return NULL;
    }
    int group = 0;
    int sub_string = 0;
    nfa_set_name(first, "G[%d]{%d}", group, sub_string);
    char *p = input;
    size_t idx = 0;
    for (; *p && idx < len; p++, idx++) {
        if (*p == '(') {
            top++;
            group++;
            sub_string++;
            if (new_empty_fragment(inner, &stack[top], group, err) != REGEX_OK) {
                free(stack);
                refree_inner(inner);
                return NULL;
            }

            struct NFA *group_first = make_node(inner, err);
            if (!group_first) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            if (add_epsilon(current.end, stack[top].start, err) != REGEX_OK) {
                free_node(stack[top].start);
                free_node(stack[top].end);
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            nfa_set_name(group_first, "G[%d]{%d}", group, sub_string);
            if (add_epsilon(stack[top].start, group_first, err) != REGEX_OK) {
                free_node(group_first);
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            current.start = group_first;
            current.end = group_first;
        } else if (*p == ')') {
            if (top == 0) {
                *err = REGEX_SYNTAX;
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            if (add_epsilon(current.end, stack[top].end, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            current = stack[top];
            top--;
        } else if (*p == '|') {
            // 对于 | 的场景，直接处理掉
            if (add_epsilon(current.end, stack[top].end, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }

            struct NFA *node = make_node(inner, err);
            if (!node) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            sub_string++;
            nfa_set_name(node, "G[%d]{%d}", group, sub_string);
            if (add_epsilon(stack[top].start, node, err) != REGEX_OK) {
                free_node(node);
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            current.start = node;
            current.end = node;
        } else if (*p == '[') {
            p++;
            idx++;
            struct NFA *node = make_node(inner, err);
            if (!node) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
            // todo: 支持 [^abc]模式 支持 [0-9]
            for (; *p != ']' && *p != '\0' && idx < len; p++, idx++) {
                current.end->next[(unsigned char) *p] = node;
                current.start = current.end;
            }
            current.end = node;
            if (idx >= len || *p != ']') {
                *err = REGEX_SYNTAX;
                refree_inner(inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '+') {
            if (add_plus(&current, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '*') {
            if (add_star(&current, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '?') {
            if (add_question(inner, &current, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '.') {
            if (add_dot(inner, &current, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '\\') {
            // todo: 增加反斜杠的处理
        } else if (32 <= *p && *p < 127) {
            if (add_literal(inner, &current, *p, err) != REGEX_OK) {
                refree_inner(inner);
                free(stack);
                return NULL;
            }
        }
    }
    if (add_epsilon(current.end, stack[top].end, err) != REGEX_OK) {
        refree_inner(inner);
        free(stack);
        return NULL;
    }
    free(stack);
    if (top != 0) {
        *err = REGEX_SYNTAX;
        refree_inner(inner);
        return NULL;
    }
    return (Regex *) inner;
}

void refree_inner(struct RegexInner *inner) {
    if (!inner) {
        return;
    }
    for (size_t i = 0; i < inner->node_count; i++) {
        free(inner->nodes[i]);
    }
    free(inner->pattern);
    free(inner->nodes);
    free(inner);
}

void refree(Regex *re) {
    refree_inner((struct RegexInner *) re);
}