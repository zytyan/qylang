//
// Created by manni on 2025/6/4.
//

#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <memory.h>
#include <ctype.h>
#include <string.h>
#include "regex.h"

#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
struct NFA {
    struct NFA *next[256];
    struct NFA **epsilon;
    size_t epsilon_count;
    bool accepted;
};

struct Fragment {
    struct NFA *start;  // 入口状态
    struct NFA *end;    // 当前末尾状态，还未确定出口连接
};

struct regex_inner {
    struct NFA *start;
    struct NFA **nodes;
    size_t node_count;
};

static struct NFA *make_node(struct regex_inner *inner, RegexErr *err) {
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
    return nfa;
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

static RegexErr add_literal(struct regex_inner *inner, struct Fragment *frag, char ch, RegexErr *err) {
    struct NFA *node = make_node(inner, err);
    if (!node) {
        return *err;
    }
    frag->end->next[(unsigned char) ch] = node;
    frag->start = frag->end;
    frag->end = node;
    return *err;
}

static RegexErr add_dot(struct regex_inner *inner, struct Fragment *frag, RegexErr *err) {
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

static RegexErr add_question(struct regex_inner *inner, struct Fragment *frag, RegexErr *err) {
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

static RegexErr add_plus(struct regex_inner *inner, struct Fragment *frag, RegexErr *err) {
    if (add_epsilon(frag->end, frag->start, err) != REGEX_OK) {
        return *err;
    }
    return *err;
}

static RegexErr add_star(struct regex_inner *inner, struct Fragment *frag, RegexErr *err) {
    if (add_epsilon(frag->end, frag->start, err) != REGEX_OK) {
        return *err;
    }
    if (add_epsilon(frag->start, frag->end, err) != REGEX_OK) {
        return *err;
    }
    return *err;
}

Regex *recompile(char *input, size_t len, RegexErr *err) {
    struct regex_inner *inner = calloc(1, sizeof(struct regex_inner));
    if (!inner) {
        return NULL;
    }
    struct NFA *const root = make_node(inner, err);
    inner->start = root;
    if (!root) {
        refree((Regex *) inner);
        return NULL;
    }
    struct Fragment *stack = (struct Fragment *) calloc(len, sizeof(struct Fragment));
    if (!stack) {
        refree((Regex *) inner);
        return NULL;
    }
    size_t top = 2;
    // stack[0] 是 dummy root
    stack[0].start = root;
    stack[0].end = root;
    struct NFA *start_node = make_node(inner, err);
    if (!start_node) {
        refree((Regex *) inner);
        free(stack);
        return NULL;
    }
    if (add_epsilon(root, start_node, err) != REGEX_OK) {
        refree((Regex *) inner);
        free(stack);
        return NULL;
    }
    // stack[1] 是当前构建中的 fragment
    stack[1].start = start_node;
    stack[1].end = start_node;

    char *p = input;
    size_t idx = 0;
    for (; *p && idx < len; p++, idx++) {
        if (*p == '(') {
            struct NFA *node = make_node(inner, err);
            if (!node) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
            if (add_epsilon(stack[top - 1].end, node, err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
            top++;
            stack[top - 1].start = node;
            stack[top - 1].end = node;
            // FIXME: 左括号的逻辑也不完善，应该多加几个epsilon转换的
        } else if (*p == ')') {
            //FIXME: 括号的逻辑还不完善，可能还涉及到管道符的逻辑修改
            stack[top - 1].end;
            top--;
        } else if (*p == '|') {
            if (top == 2) {
                // 如果 | 没有括号，那就认为是接收状态
                stack[top - 1].end->accepted = true;
            }
            struct NFA *node = make_node(inner, err);
            if (!node) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
            if (add_epsilon(stack[top - 2].end, node, err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
            stack[top - 1].start = stack[top - 2].end;
            stack[top - 1].end = node;
        } else if (*p == '[') {
            p++;
            idx++;
            struct NFA *node = make_node(inner, err);
            if (!node) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
            // todo: 支持 [^abc]模式 支持 [0-9]
            for (; *p != ']' && *p != '\0' && idx < len; p++, idx++) {
                stack[top - 1].end->next[(unsigned char) *p] = node;
                stack[top - 1].start = stack[top - 1].end;
            }
            stack[top - 1].end = node;
            if (idx >= len || *p != ']') {
                *err = REGEX_SYNTAX;
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '+') {
            if (add_plus(inner, &stack[top - 1], err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '*') {
            if (add_star(inner, &stack[top - 1], err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '?') {
            if (add_question(inner, &stack[top - 1], err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '.') {
            if (add_dot(inner, &stack[top - 1], err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
        } else if (*p == '\\') {
            // todo: 增加反斜杠的处理
        } else if (32 <= *p && *p < 127) {
            if (add_literal(inner, &stack[top - 1], *p, err) != REGEX_OK) {
                refree((Regex *) inner);
                free(stack);
                return NULL;
            }
        }
    }
    stack[top - 1].end->accepted = true;
    free(stack);
    if (top != 2) {
        refree((Regex *) inner);
        return NULL;
    }
    return (Regex *) inner;
}


void refree(Regex *re) {
    if (!re) {
        return;
    }
    struct regex_inner *inner = (struct regex_inner *) re;
    for (size_t i = 0; i < inner->node_count; i++) {
        free(inner->nodes[i]);
    }
    free(inner->nodes);
    free(inner);
}

#define MAX_STATES 1024  // 可根据需求调整

static void add_state(struct NFA **states, size_t *count, struct NFA *state, struct NFA **visited, size_t *vcount) {
    // 去重
    for (size_t i = 0; i < *vcount; i++) {
        if (visited[i] == state) return;
    }
    visited[(*vcount)++] = state;

    // 加入状态集
    states[(*count)++] = state;

    // 展开 ε 转移
    for (size_t i = 0; i < state->epsilon_count; i++) {
        add_state(states, count, state->epsilon[i], visited, vcount);
    }
}

bool rematch(Regex *re, char *const string, RegexErr *err) {
    if (!re) {
        return false;
    }
    struct regex_inner *inner = (struct regex_inner *) re;

    struct NFA *current[MAX_STATES];
    struct NFA *next[MAX_STATES];
    struct NFA *visited[MAX_STATES];

    size_t current_count = 0;
    size_t next_count = 0;
    size_t visited_count = 0;

    // 初始状态闭包
    add_state(current, &current_count, inner->start, visited, &visited_count);

    for (char *p = string; *p; p++) {
        next_count = 0;
        visited_count = 0;

        for (size_t i = 0; i < current_count; i++) {
            struct NFA *s = current[i];
            struct NFA *dst = s->next[(unsigned char) *p];
            if (dst) {
                add_state(next, &next_count, dst, visited, &visited_count);
            }
        }

        memcpy(current, next, sizeof(struct NFA *) * next_count);
        current_count = next_count;
    }

    // 检查是否有接受状态
    for (size_t i = 0; i < current_count; i++) {
        if (current[i]->accepted) return true;
    }

    return false;
}

static void write_ranges(FILE *out, const bool *marks) {
    int i = 0;
    bool first = true;
    while (i < 256) {
        if (!marks[i]) {
            i++;
            continue;
        }

        int start = i;
        while (i + 1 < 256 && marks[i + 1]) {
            i++;
        }
        int end = i;

        if (!first) fprintf(out, ", ");
        if (start == end) {
            if (isprint(start)) {
                fprintf(out, "%c", start);
            } else {
                fprintf(out, "0x%02X", start);
            }
        } else {
            if (isprint(start)) {
                fprintf(out, "%c-", start);
            } else {
                fprintf(out, "0x%02X-", start);
            }
            if (isprint(end)) {
                fprintf(out, "%c", end);
            } else {
                fprintf(out, "0x%02X", end);
            }
        }

        i++;
        first = false;
    }
}

void dump_dot(Regex *re, FILE *out) {
    if (!re || !out) return;
    struct regex_inner *inner = (struct regex_inner *) re;

    fprintf(out, "digraph NFA {\n");
    fprintf(out, "  rankdir=LR;\n");

    // 输出节点定义
    for (size_t i = 0; i < inner->node_count; i++) {
        struct NFA *nfa = inner->nodes[i];
        fprintf(out, "  node%p [shape=%s label=\"%p\"];\n",
                (void *) nfa,
                nfa->accepted ? "doublecircle" : "circle",
                (void *) nfa);
    }

    // 输出边
    for (size_t i = 0; i < inner->node_count; i++) {
        struct NFA *nfa = inner->nodes[i];

        // 合并 next 指向相同 dst 的字符集合
        bool emitted[256] = {0};

        for (int ch = 0; ch < 256; ch++) {
            if (!nfa->next[ch] || emitted[ch]) continue;

            struct NFA *dst = nfa->next[ch];
            bool used[256] = {0};

            // 收集所有指向 dst 的字符
            for (int j = ch; j < 256; j++) {
                if (nfa->next[j] == dst) {
                    used[j] = true;
                    emitted[j] = true;
                }
            }

            fprintf(out, "  node%p -> node%p [label=\"", (void *) nfa, (void *) dst);
            write_ranges(out, used);
            fprintf(out, "\"];\n");
        }

        // ε 转移
        for (size_t j = 0; j < nfa->epsilon_count; j++) {
            fprintf(out, "  node%p -> node%p [label=\"ε\"];\n",
                    (void *) nfa, (void *) nfa->epsilon[j]);
        }
    }

    fprintf(out, "}\n");
}

int main() {
    RegexErr err = REGEX_OK;
    Regex *re = recompile("ab|cd", strlen("ab|cd"), &err);
    if (!re) {
        return 0;
    }
    printf("%d\n", rematch(re, "ab", &err));  // 1
    printf("%d\n", rematch(re, "abc", &err));  // 0
    printf("%d\n", rematch(re, "abd", &err));  // 0
    printf("%d\n", rematch(re, "abe", &err));  // 0
    printf("%d\n", rematch(re, "abcccc", &err));   // 0
    printf("%d\n", rematch(re, "abc", &err));   // 0
    printf("%d\n", rematch(re, "cd", &err)); // 1
    FILE *fp = fopen("test.dot", "w");
    if (!fp) {
        perror("open test.dot failed");
    }
    dump_dot(re, fp);
    fclose(fp);
    refree(re);
}