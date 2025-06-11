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

void fprint_str_escaped(FILE *fp, const char *data, const size_t len) {
    size_t pos = 0;
    for (const char *p = data; *p && pos < len; ++p, ++pos) {
        if (*p == '"') {
            fputc('\\', fp);
        }
        fputc(*p, fp);
    }
}

void dump_dot(Regex *re, FILE *out) {
    if (!re || !out)
        return;
    struct RegexInner *inner = (struct RegexInner *) re;

    fprintf(out, "digraph NFA {\n");
    fprintf(out, "  rankdir=LR;\n");
    fprintf(out, "  labelloc = \"t\";\n");
    fprintf(out, "  label = \"");
    fprint_str_escaped(out, inner->name, sizeof(inner->name));
    fprintf(out, "\";\n");
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
    Regex *re = REC("ab(a[bd]*|cd)?|xqwyz", &err);
    if (!re) {
        return 0;
    }
    printf("%d\n", refullmatch(re, "abab", &err));      // 1
    printf("%d\n", refullmatch(re, "abad", &err));      // 1
    printf("%d\n", refullmatch(re, "ababad", &err));    // 0
    printf("%d\n", refullmatch(re, "abadcd", &err));    // 0
    printf("%d\n", refullmatch(re, "ababdabd", &err));  // 0
    printf("%d\n", refullmatch(re, "ab", &err));        // 1
    printf("%d\n", refullmatch(re, "abcccc", &err));    // 0
    printf("%d\n", refullmatch(re, "abc", &err));       // 0
    printf("%d\n", refullmatch(re, "cd", &err));        // 0
    printf("%d\n", refullmatch(re, "xqwyz", &err));       // 1
    FILE *fp = fopen("test.dot", "w");
    if (!fp) {
        perror("open test.dot failed");
    }
    dump_dot(re, fp);
    fclose(fp);
    refree(re);
}
