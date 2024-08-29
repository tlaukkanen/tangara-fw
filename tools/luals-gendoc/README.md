# Prerequisites

 - lua-language-server
 - a Lua interpreter version >= 5.2

# Regenerating the Lua reference docs

1. Invoke `lua-language-server` to parse our stubs, generating json output:

```
$ lua-language-server --doc=../../luals-stubs
```

2. The output will include the path to the raw json data. This will be a path like '/home/jacqueline/Development/lua-language-server/log/doc.json'. Pipe this file to `gendoc.lua`. The output will be quite long; consider piping to less for testing.

```
$ cat /home/jacqueline/Development/lua-language-server/log/doc.json | ./gendoc.lua | less
```

3. If updating the public-facing docs on cooltech.zone, place the resulting output in 'content/tangara/docs/lua/reference.txt'.
