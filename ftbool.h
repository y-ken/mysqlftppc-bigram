#include <my_global.h>
#include <m_ctype.h>

// char sequence flow control definitions
typedef enum seqflow {
    SF_CHAR,
    SF_ESCAPE,
    SF_QUOTE_START,
    SF_QUOTE_END,
    SF_LEFT_PAREN,
    SF_RIGHT_PAREN,
    SF_WHITE,
    SF_PLUS,
    SF_MINUS,
    SF_WEAK,
    SF_STRONG,
    SF_WASIGN,
    SF_TRUNC,
    SF_BROKEN
} SEQFLOW;

// char sequence context FLAGS
static int CTX_ESCAPE  = 1;
static int CTX_QUOTE   = 2;
static int CTX_CONTROL = 4;

SEQFLOW ctxscan(CHARSET_INFO *cs, char *src, char *src_end, my_wc_t *dst, int *readsize, int context);
