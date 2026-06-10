# mt

mt is a feature-rich, state-of-the-art, Turing complete language designed for people who think programming is too dynamic.

it gives you, get this, not one, not two, but **THREE** types of tokens.

## what is this about

everything in mt is whitespace. space, tab, newline. that's it. your program is invisible.

this is a (shitty) encoding of binary lambda calculus where space and newline are both the bit `0`, but they play different roles so the bitstream is actually parseable by a human. groundbreaking, i know.

## the three marvels of computation

| pattern | what is this |
| ---------------------------- | ------------------------------- |
| `  ` (space space)           | lambda abstraction              |
| ` \t` (space tab)            | application                     |
| `\t`* `\n` (n tabs, newline) | variable with de Bruijn index n |

your program is a closed de bruijn-indexed lambda term. it gets normalized. then:

* if the result is a church numeral N, raw byte N to stdout
* if the result is a church list of numerals, all bytes in sequence to stdout
* anything else, nothing.

## execution

```
$ make
$ ./mt whatever.mt
```

## hello world

`hello.mt` is right here.

```
$ ./mt hello.mt
hello world
```

## how is this turing complete?

it's Turing complete because BLC is Turing complete, and this is BLC with worse variable terminators.

that's it. thank you for coming to my TED talk.
