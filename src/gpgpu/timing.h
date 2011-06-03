double timeval_diff(const struct timeval * const start, const struct timeval * const end);
void print_timing(const struct timeval * const start, const struct timeval * const end, const char * start_label, const char * end_label);
void timing_stamp(char *name, int done);
void timing_report();
