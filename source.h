#ifndef MGCC_SOURCE_H
#define MGCC_SOURCE_H

struct source_loc {
    const char *filename;
    int line;
    int col;
};

#endif
