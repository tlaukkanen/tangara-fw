#!/bin/python3

def parse_charmap(filename):
    # Ref: https://pubs.opengroup.org/onlinepubs/9699919799/
    # https://codebrowser.dev/glibc/glibc/string/strxfrm_l.c.html#__strxfrm_l
    comment_char = "#"
    escape_char = "\\"
    mb_cur_min = 1
    mb_cur_max = 1

    bool in_charmap = false
    with open(filename, "r") as f:
        for line in f:
            pass