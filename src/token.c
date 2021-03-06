/*
 * Copyright (C) 2011-2015 goblinhack@gmail.com
 *
 * See the README file for license info for license.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "slre.h"

#include "main.h"
#include "token.h"

static tokens_t *tokens_parse (const char *input, tokens_t *tokens)
{
    const char *in;
    char *out;
    char i;

    memset(tokens, 0, sizeof(*tokens));

    if (strlen(input) >= MAXSTR) {
        return (tokens);
    }

    in = input;
    out = tokens->data;
    *out = '\0';

    for (;;) {
        /*
         * Check we have token space.
         */
        if (tokens->cnt >= ARRAY_SIZE(tokens->args)) {
            tokens->cnt = 0;
            return (tokens);
        }

        i = *(in++);

        /*
         * End of line ?
         */
        if (i == '\0') {
            return (tokens);
        }

        /*
         * Skip leading pad.
         */
        while ((i == ' ') || (i == '\t')) {
            i = *(in++);
        }

        /*
         * End of line ?
         */
        if (i == '\0') {
            return (tokens);
        }

        /*
         * Read whole strings.
         */
        if (i == '\"') {
            tokens->args[tokens->cnt++] = out;

            for (;;) {
                i = *(in++);
                switch (i) {
                case '\\':
                    i = *(in++);
                    switch (i) {
                        case '\"':
                            i = '\"';
                        break;
                        case '\n':
                            i = '\n';
                            break;
                        case '\r':
                            i = '\r';
                            break;
                        case '\t':
                            i = '\t';
                            break;
                        case '\\':
                            i = '\\';
                            break;
                        default:
                            break;
                    }
                    break;

                case '\"':
                    if (i == '\"') {
                        i = '\0';
                        break;
                    }
                    break;
                }

                *out++ = i;

                if (i == '\0') {
                    break;
                }
            }

            continue;
        }

        /*
         * End of line ?
         */
        if (i == '\0') {
            return (tokens);
        }

        tokens->args[tokens->cnt++] = out;

        for (;;) {
            switch (i) {
                case '\0':
                    *out++ = i;
                    return (tokens);

                case ' ':
                    i = '\0';
                    break;

                case '\t':
                    i = '\0';
                    break;

                default:
                    break;
            }

            *out++ = i;

            if (i == '\0') {
                break;
            }

            i = *(in++);
        }
    }
}

static void tokens_compile (tokens_t *tokens)
{
    uint32_t cnt;

    cnt = 0;

    while (cnt < tokens->cnt) {
        if (!slre_compile(&tokens->regexp[cnt], tokens->args[cnt])) {
            ERR("failed to compile \"%s\"", tokens->args[cnt]);
        }
        cnt++;
    }
}

void tokens_print (tokens_t *tokens)
{
    uint32_t cnt;

    printf("tokens %u: ", tokens->cnt);
    cnt = 0;

    while (cnt < tokens->cnt) {
        printf("[%s] ", tokens->args[cnt]);
        cnt++;
    }

    printf("\n");
}

void tokens_print_to (tokens_t *tokens, char *output, int32_t output_size)
{
    uint32_t cnt;

    cnt = 0;
    *output = '\0';

    while (cnt < tokens->cnt) {
        strlcat(output, tokens->args[cnt], output_size);
        cnt++;

        if (cnt < tokens->cnt) {
            strlcat(output, " ", output_size);
        }
    }
}

void tokens_test (void)
{
    tokens_t tmp;

    tokens_print(tokens_tostring("s  ", &tmp));
    tokens_print(tokens_tostring("set god mode on", &tmp));
    tokens_print(tokens_tostring("set god \"mode on\"", &tmp));
}

tokens_t *tokens_tostring (const char *input, tokens_t *tokens)
{
    tokens_t *t;

    t = tokens_parse(input, tokens);
    tokens_compile(tokens);

    return (t);
}
