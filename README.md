# ctrie

High performance, low memory consumption compact trie data structure written in C. 

## WARNING

I never needed deletion, so I never implemented it. Send a patch if you do.

## DESCRIPTION

A [compact trie](https://en.wikipedia.org/wiki/Radix_tree) is
a [trie](https://en.wikipedia.org/wiki/Trie) where each node which is the only
child is merged with its parent. This allows for substantial memory savings
compared to the naive trie implementation, while preserving decent speed.

This implementation further saves space by directly embedding short labels
in the trie nodes whenever possible, saving heap allocations and the associated
overhead of accounting data (i.e. pointers to strings) which need to be kept.

The code is clean and well tested.

## AUTHORS

 - David Čepelík <david.cepelik@showmax.com> (c) 2018
 - Ondřej Hrubý <o@hrubon.cz> (c) 2018
