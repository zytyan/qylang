//
// Created by manni on 2025/6/5.
//

#ifndef QYLANG_RECOMPILE_H
#define QYLANG_RECOMPILE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
struct NFA {
    struct NFA *next[256];
    struct NFA **epsilon;
    size_t epsilon_count;
    bool accepted;
    char name[15];
};

struct Fragment {
    struct NFA *start;  // 入口状态
    struct NFA *end;    // 当前末尾状态，还未确定出口连接
};

struct RegexInner {
    struct NFA *start;
    struct NFA **nodes;
    size_t node_count;
    char *pattern;
};

void refree_inner(struct RegexInner *inner);

#endif //QYLANG_RECOMPILE_H
