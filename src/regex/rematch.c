//
// Created by manni on 2025/6/6.
//
#include <memory.h>
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

bool refullmatch(Regex *re, char *const string, RegexErr *err) {
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

    // ✅ 处理末尾 ε 转移
    visited_count = 0;
    size_t final_count = 0;
    struct NFA *final[MAX_STATES];
    for (size_t i = 0; i < current_count; i++) {
        add_state(final, &final_count, current[i], visited, &visited_count);
    }

    // ✅ 检查接受状态
    for (size_t i = 0; i < final_count; i++) {
        if (final[i]->accepted)
            return true;
    }

    return false;
}
