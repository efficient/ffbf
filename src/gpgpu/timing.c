#include <sys/time.h>
#include <string.h>

#include <stdio.h>

#include "timing.h"

/* WARNING - XXX - timing stamp functions are not reentrant.  The buffer
 * can easily overflow if you take too many.  This is a stub to be fixed
 * later. */

static char timebuf[4096];
static int initted = 0;
static struct timeval tv_start, tv_end;

void
timing_stamp(char *name, int done)
{
  struct timeval tv_now;
  static struct timeval tv_prev;
  gettimeofday(&tv_now, NULL);
  if (!initted) { 
    bzero(timebuf, sizeof(timebuf));
    tv_start = tv_now;
  }

  if (initted) {
    char *b = timebuf + strlen(timebuf);
    sprintf(b, "%-15.15s: %2.2f\n", name, timeval_diff(&tv_prev, &tv_now));
  }
  initted = 1;

  if (!done) {
    tv_prev = tv_now;
    char *b = timebuf + strlen(timebuf);
    sprintf(b, "%-15.15s -> ", name);
  } else {
    tv_end = tv_now;
  }
}

void timing_report() {
  printf("\nTiming Report\n---------\n");
  printf("%s", timebuf);
  printf("\n");
  printf("Total time: %2.2f\n", timeval_diff(&tv_start, &tv_end));
}
  

double timeval_diff(const struct timeval * const start, const struct timeval * const end)
{
    /* Calculate the second difference*/
    double r = end->tv_sec - start->tv_sec;

    /* Calculate the microsecond difference */
    if (end->tv_usec > start->tv_usec)
        r += (end->tv_usec - start->tv_usec)/1000000.0;
    else if (end->tv_usec < start->tv_usec)
        r -= (start->tv_usec - end->tv_usec)/1000000.0;

    return r;
}

void print_timing(const struct timeval * const start, const struct timeval * const end, const char * start_label, const char * end_label)
{
  printf("Time from %s to %s: %2.2f\n", start_label, end_label,
	 timeval_diff(start, end));
}
