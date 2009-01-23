typedef struct _ftstring_buffer {
  char* start;
  int   length;
  char* buffer;
  int   buffer_length;
  int   rewritable;
} FTSTRING;

char* ftstring_head(FTSTRING *str);
int   ftstring_length(FTSTRING *str);
void  ftstring_destroy(FTSTRING *str);
void  ftstring_append(FTSTRING *str, char* src, int length);
void  ftstring_bind(FTSTRING *str, char* src, int rewritable);
void  ftstring_unbind(FTSTRING *str);
void  ftstring_reset(FTSTRING *str);
int   ftstring_internal(FTSTRING *str);
