/**
 * @param src source UTF-8 string pointer
 * @param src_len source UTF-8 string length (byte length)
 * @param dst normalized UTF-8 string pointer
 * @param dst_capacity normalized UTF-8 string length (byte length)
 * @param mode normalization mode
 * @param options normalization options
 * @return actual size of written. 0 on failure.
 */
size_t uni_normalize(char* src, size_t src_len, char* dst, size_t dst_capacity, int mode, int options);
