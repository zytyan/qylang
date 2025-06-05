//
// Created by manni on 2025/6/4.
//

#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <ctype.h>
#include <string.h>
#include "regex.h"
#include "recompile.h"

#define MAX_STATES 1024  // 可根据需求调整

static void add_state(struct NFA **states, size_t *count, struct NFA *state, struct NFA **visited, size_t *vcount) {
    // 去重
    for (size_t i = 0; i < *vcount; i++) {
        if (visited[i] == state)
            return;
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
    struct RegexInner *inner = (struct RegexInner *) re;

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
        if (current[i]->accepted)
            return true;
    }

    return false;
}

static void write_char(FILE *out, int ch) {
    if (ch == '"' || ch == '\\') {
        fprintf(out, "\\%c", ch);
    } else if (isprint(ch)) {
        fputc(ch, out);
    } else {
        fprintf(out, "\\x%02X", ch);
    }
}

static void write_ranges(FILE *out, const bool *used) {
    bool first = true;
    for (int i = 0; i < 256;) {
        if (!used[i]) {
            i++;
            continue;
        }

        int start = i;
        while (i + 1 < 256 && used[i + 1]) {
            i++;
        }
        int end = i;

        if (!first)
            fprintf(out, ", ");

        write_char(out, start);
        if (start != end) {
            fputc('-', out);
            write_char(out, end);
        }

        first = false;
        i++;
    }
}

void dump_dot(Regex *re, FILE *out) {
    if (!re || !out)
        return;
    struct RegexInner *inner = (struct RegexInner *) re;

    fprintf(out, "digraph NFA {\n");
    fprintf(out, "  rankdir=LR;\n");

    // 输出节点定义
    for (size_t i = 0; i < inner->node_count; i++) {
        struct NFA *nfa = inner->nodes[i];
        fprintf(out,
                "  node%p [shape=%s label=\"%s\"];\n",
                (void *) nfa,
                nfa->accepted ? "doublecircle" : "circle",
                nfa->name);
    }

    // 输出边
    for (size_t i = 0; i < inner->node_count; i++) {
        struct NFA *nfa = inner->nodes[i];

        // 合并 next 指向相同 dst 的字符集合
        bool emitted[256] = {0};

        for (int ch = 0; ch < 256; ch++) {
            if (!nfa->next[ch] || emitted[ch])
                continue;

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
            fprintf(out,
                    "  node%p -> node%p [label=\"ε\", style=dashed, color=red];\n",
                    (void *) nfa,
                    (void *) nfa->epsilon[j]);
        }
    }

    fprintf(out, "}\n");
}

#define REC(s, e) recompile((s), strlen(s), (e));

int main() {
    RegexErr err = REGEX_OK;
    Regex *re = REC("(ab|cd|ef|gh|dot(py|exe|dll)end)m(de|ps|ls)", &err);
    if (!re) {
        return 0;
    }
    printf("%d\n", rematch(re, "abab", &err));      // 1
    printf("%d\n", rematch(re, "abad", &err));      // 1
    printf("%d\n", rematch(re, "ababad", &err));    // 1
    printf("%d\n", rematch(re, "abadcd", &err));    // 1
    printf("%d\n", rematch(re, "ababdabd", &err));  // 0
    printf("%d\n", rematch(re, "ab", &err));        // 1
    printf("%d\n", rematch(re, "abcccc", &err));    // 0
    printf("%d\n", rematch(re, "abc", &err));       // 0
    printf("%d\n", rematch(re, "cd", &err));        // 0
    printf("%d\n", rematch(re, "xqwyz", &err));       // 1
    FILE *fp = fopen("test.dot", "w");
    if (!fp) {
        perror("open test.dot failed");
    }
    dump_dot(re, fp);
    fclose(fp);
    refree(re);
}
