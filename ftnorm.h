/**
 * @param src source UTF-8 string pointer
 * @param src_len source UTF-8 string length (byte length)
 * @param dst normalized UTF-8 string pointer
 * @param dst_capacity normalized UTF-8 string length (byte length)
 * @param dst_used the length will be written
 */
char* uni_normalize(char* src, size_t src_len, char* dst, size_t dst_capacity, size_t *dst_used, int mode, int* status);
