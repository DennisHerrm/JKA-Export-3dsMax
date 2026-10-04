# g2c (Ghoul2 writers)

From **g2c**, DennisHerrm's re-implementation of Raven's `carcass.exe` (Ghoul2 toolkit), taken
unchanged:

    include/g2/  model.h mdxa.h mdxm.h compress.h format.h bytebuf.h parallel.h
    src/         mdxa.cpp mdxm.cpp compress.cpp parallel_stub.cpp

Only `writeMdxa` (GLA) and `writeMdxm` (GLM) are used. g2c verified its formulas and the file
layout against Raven's original game files. The file layout itself follows Raven's
`mdx_format.h` from the Jedi Academy source release (GPLv2); g2c does not copy Raven source code.

Built as the C++20 library `g2export`; the plugin only sees `src/g2export.h`.
