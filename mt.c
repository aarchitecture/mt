#include <stdio.h>
#include <stdlib.h>

typedef enum { NL, SP, TB, EOI } Token;

// lambda term
typedef struct Term {
    enum { VAR, ABS, APP } kind;
    int idx;
    struct Term *left;
    struct Term *right;
} Term;

// arena
#define ARENA_CAP (2 * 1024 * 1024)
static char arena_buf[ARENA_CAP];
static size_t arena_off;

static Term *alloc_term(void) {
    if (arena_off + sizeof(Term) > ARENA_CAP) {
        fprintf(stderr, "fatal: arena exhausted\n");
        exit(1);
    }
    Term *t = (Term *)(arena_buf + arena_off);
    arena_off += sizeof(Term);
    return t;
}

static Term *make_var(int idx) {
    Term *t = alloc_term();
    t->kind = VAR;
    t->idx = idx;
    t->left = t->right = NULL;
    return t;
}

static Term *make_abs(Term *body) {
    Term *t = alloc_term();
    t->kind = ABS;
    t->left = body;
    t->right = NULL;
    return t;
}

static Term *make_app(Term *left, Term *right) {
    Term *t = alloc_term();
    t->kind = APP;
    t->left = left;
    t->right = right;
    return t;
}

// lexer
static Token toks[65536];
static int ntoks;
static int tokpos;

static Token peek(void) {
    if (tokpos >= ntoks) return EOI;
    return toks[tokpos];
}

static void advance(void) {
    tokpos++;
}

static void lex(const char *buf, size_t len) {
    ntoks = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = buf[i];
        if (c == ' ')           toks[ntoks++] = SP;
        else if (c == '\t')     toks[ntoks++] = TB;
        else if (c == '\n')     toks[ntoks++] = NL;
    }
}

// parser
static Term *parse_term(void);

static Term *parse_var(void) {
    int count = 0;
    while (peek() == TB) {
        count++;
        advance();
    }
    if (peek() != NL) {
        fprintf(stderr, "parse error: expected newline after tab(s)\n");
        exit(1);
    }
    advance();
    return make_var(count);
}

static Term *parse_term(void) {
    Token t = peek();
    if (t == SP) {
        advance();
        Token t2 = peek();
        if (t2 == SP) {
            advance();
            Term *body = parse_term();
            return make_abs(body);
        } else if (t2 == TB) {
            advance();
            Term *left = parse_term();
            Term *right = parse_term();
            return make_app(left, right);
        } else {
            fprintf(stderr, "parse error: expected space or tab after space\n");
            exit(1);
        }
    } else if (t == TB || t == NL) {
        return parse_var();
    } else {
        fprintf(stderr, "parse error: unexpected character\n");
        exit(1);
    }
}

// de bruijn shift (shift all free vars w/ index >= c by d)
static Term *shift_into(Term *t, int d, int c) {
    switch (t->kind) {
        case VAR:
            if (t->idx >= c)
                return make_var(t->idx + d);
            return t;
        case ABS:
            return make_abs(shift_into(t->left, d, c + 1));
        case APP:
            return make_app(
                shift_into(t->left, d, c),
                shift_into(t->right, d, c));
    }
    return t;
}

static Term *shift(Term *t, int d) {
    return shift_into(t, d, 0);
}

// de bruijn sub (sub N for variable index k in M)
static Term *subst(Term *m, int k, Term *n) {
    switch (m->kind) {
        case VAR:
            if (m->idx < k)
                return m;
            if (m->idx == k)
                return n;
            return make_var(m->idx - 1);
        case ABS:
            return make_abs(subst(m->left, k + 1, shift(n, 1)));
        case APP:
            return make_app(
                subst(m->left, k, n),
                subst(m->right, k, n));
    }
    return m;
}

// normal-order reduction
static Term *normalize(Term *t) {
    switch (t->kind) {
        case VAR:
            return t;
        case ABS:
            return make_abs(normalize(t->left));
        case APP: {
            Term *left = normalize(t->left);
            if (left->kind == ABS) {
                Term *reduced = subst(left->left, 0, t->right);
                return normalize(reduced);
            }
            Term *right = normalize(t->right);
            return make_app(left, right);
        }
    }
    return t;
}

// free var check
static int check_closed(Term *t, int binders) {
    switch (t->kind) {
        case VAR:
            if (t->idx >= binders) {
                fprintf(stderr, "error: free variable %d (only %d binders)\n",
                        t->idx, binders);
                return 0;
            }
            return 1;
        case ABS:
            return check_closed(t->left, binders + 1);
        case APP:
            return check_closed(t->left, binders) &&
                   check_closed(t->right, binders);
    }
    return 0;
}

// church numeral/list decoder
static int count_church(Term *t, int n) {
    if (t->kind == VAR && t->idx == 0) return n;
    if (t->kind != APP) return -1;
    if (t->left->kind != VAR || t->left->idx != 1) return -1;
    return count_church(t->right, n + 1);
}

static int decode_numeral(Term *t) {
    if (t->kind != ABS) return -1;
    if (t->left->kind != ABS) return -1;
    return count_church(t->left->left, 0);
}

static int decode_body(Term *b) {
    if (b->kind == VAR && b->idx == 0) return 0;
    if (b->kind != APP) return -1;
    Term *la = b->left;
    if (la->kind != APP) return -1;
    if (la->left->kind != VAR || la->left->idx != 1) return -1;

    int ch = decode_numeral(la->right);
    if (ch < 0) return -1;
    fputc(ch, stdout);

    int rest = decode_body(b->right);
    if (rest < 0) return -1;
    return rest + 1;
}

static int decode_list(Term *t) {
    if (t->kind != ABS) return -1;
    if (t->left->kind != ABS) return -1;
    return decode_body(t->left->left);
}

int main(int argc, char **argv) {
    FILE *fp = stdin;
    if (argc > 1) {
        fp = fopen(argv[1], "r");
        if (!fp) {
            perror(argv[1]);
            return 1;
        }
    }

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    if (n == 0 && !feof(fp)) {
        fprintf(stderr, "error reading input\n");
        return 1;
    }
    buf[n] = '\0';

    if (argc > 1) fclose(fp);

    lex(buf, n);

    if (ntoks == 0) {
        fprintf(stderr, "error: empty program\n");
        return 1;
    }

    tokpos = 0;
    Term *prog = parse_term();

    if (tokpos != ntoks) {
        fprintf(stderr, "error: unexpected extra tokens after term\n");
        return 1;
    }

    if (!check_closed(prog, 0))
        return 1;

    Term *nf = normalize(prog);

    int ch = decode_numeral(nf);
    if (ch >= 0) {
        fputc(ch, stdout);
    } else {
        decode_list(nf); // bytes written to stdout if applicable
    }

    return 0;
}
