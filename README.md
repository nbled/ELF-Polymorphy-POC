# ELF-Polymorphy-POC
POC of a polymorphic ELF.
This program change his own signature by rewriting a specific section (.poly)

# Compile it
`gcc -s -Wall -Wextra -Werror -Wl,--build-id=none -o tanuki tanuki.c`

# Demo
![Demo](https://i.imgur.com/PYclH9i.gif)