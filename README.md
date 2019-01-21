# ctrie

High performance, low memory consumption compact trie data structure written in C. 

## WIP WARNING

Serious limitations:

 - I never needed deletion, so I never implemented it.

I'll add missing features when I need them. If you need them earlier, please send
a patch and tests for the new functionality, I'll be happy to merge it ASAP.

## DESCRIPTION

A [compact trie](https://en.wikipedia.org/wiki/Radix_tree) is a
[trie](https://en.wikipedia.org/wiki/Trie) where each node which is the only
child is merged with its parent. This allows for substantial memory savings
compared to the naive trie implementation, while preserving decent speed.

This implementation further saves space by directly embedding short labels in
the trie nodes whenever possible, saving heap allocations and the associated
overhead of accounting data (i.e. pointers to strings) which need to be kept.

The code is clean and well tested.

## FEATURES

 - Fast insertion and lookup (*O(k)* where *k* is the length of the key in bytes)
 - Well tested
 - Wildcard support

### Wildcards

It's possible to insert prefix nodes. For example, you may insert a `foobar*`
node, which will be returned upon looking for any key starting with `foobar`,
e.g. `foobar` or `foobarbaz`.

## AUTHORS

 - David Čepelík
 - Ondřej Hrubý
