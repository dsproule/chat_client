#ifndef UTIL_H
#define UTIL_H

// All values must be -1 because indexing starts from 0
// Experimentally determined 19 x 50
// Buffer allows for 22 x 64 

#define MAX_COL 64
#define MAX_ROW 22

void clear_section(int row_start, int row_end, int col_start, int col_end);

#endif
