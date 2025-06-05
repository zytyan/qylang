//
// Created by manni on 2025/6/4.
//

#ifndef QYLANG_REGEX_H
#define QYLANG_REGEX_H

#include <stdbool.h>

typedef void Regex;
typedef enum {
    REGEX_OK,
    REGEX_NOMEM,
    REGEX_SYNTAX,
} RegexErr;

Regex *recompile(char *const input, size_t len, RegexErr *err);

void refree(Regex *re);

bool refullmatch(Regex *re, char *const string, RegexErr *err);

bool rematch(Regex *re, char *const string, RegexErr *err);

#endif //QYLANG_REGEX_H
