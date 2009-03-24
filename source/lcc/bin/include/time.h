/* time.h Struct and function declarations for dealing with time. */
#ifndef _TIME_H_
#define _TIME_H_
typedef unsigned long clock_t;
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef unsigned long time_t;
#endif
#define CLOCKS_PER_SEC 1000		/* Machine dependent */
#define _TIME_T_DEFINED
struct tm
{
  int	tm_sec;
  int	tm_min;
  int	tm_hour;
  int	tm_mday;
  int	tm_mon;
  int	tm_year;
  int	tm_wday;
  int	tm_yday;
  int	tm_isdst;
};
unsigned int clock(void);
double	   difftime(time_t _time2, time_t _time1);
time_t mktime(struct tm *_timeptr);
time_t time(clock_t *_timer);
char	  *asctime(const struct tm *_tblock);
char	  *ctime(time_t *_time);
struct tm *gmtime(time_t *_timer);
struct tm *localtime(time_t *_timer);
unsigned int   strftime(char *_s, unsigned long _maxsize, char *_fmt, struct tm *_t);
char	  *_asctime_r(struct tm *_tblock, void *_p);
extern int _daylight;
extern int *(_imp___daylight);
#define _daylight *(_imp___daylight)
char *_strtime(char *);
char *strtime(char *);
#define strtime _strtime
char *_strdate(char *);
#define strdate _strdate
extern long _timezone;
extern long *(_imp___timezone);
#define _timezone *(_imp___timezone)
extern char *_tzname[2];
extern char **(_imp___tzname)[2];
#define _tzname *(_imp___tzname)
void _tzset(void);
#endif /* _TIME_H_ */
