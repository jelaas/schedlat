/*
 * File: schedlat.c
 * Implements: scheduling latency measurements
 *
 * Copyright: Jens Låås, 2009-2014
 * Copyright license: According to GPL, see file COPYING in this directory.
 *
 */

#define _GNU_SOURCE
#include <stdio.h> 
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/resource.h>

#include <sched.h>
#include <stdlib.h>

#include "jelopt.h"

#define VALGROUP 10
#define HISTLEN 100
#define HISTSIZE (VALGROUP*HISTLEN)

#define MAXLAT 1
#define MINLAT 5000000


struct  {
	int interval;
	int verbose;
	int average;
	int pause;
	int prio;
} conf;

static int64_t average(int64_t *hist, int samples)
{
        int64_t avg;
	int g;
	
	/* aggregate results */
	avg = 0;
	for(g=0;g<samples;g++) {
		avg += hist[g];
		if(avg < hist[g]) return -1;
	}
	avg = avg / samples;
	return avg;
}

int compar(const void *i1, const void *i2)
{
	const int64_t *i = i1, *j = i2;
	return *i - *j;
}

int64_t median(int64_t *hist, int samples)
{
	qsort(hist, samples, sizeof(int64_t), compar);
	return hist[samples>>1];
}

/*
 Use clock_gettime() instead, which may give nanosecond resolution?

 */

int cpumain(int cpu, int ncpu)
{
	struct timeval tv, prev_tv, sample_tv;
        int64_t diff;
        uint32_t i,ii;
        int32_t maxlat = MAXLAT;
        int32_t minlat = MINLAT;
	cpu_set_t cpumask;
	int64_t *hist;
	int ih, samples=0;
	int calibrate=1;
	int histsize=0;
	int overflows=0;

	CPU_ZERO(&cpumask);
	CPU_SET(cpu, &cpumask);

	if(sched_setaffinity(0, sizeof(cpumask), &cpumask)) {
		fprintf(stderr, "Error: sched_setaffinity(cpu=%d) failed!\n", cpu);
		return -1;
	}
	
	sleep(1); /* allow process to migrate */
	
	gettimeofday(&prev_tv, NULL);
	sample_tv = prev_tv;
	sample_tv.tv_sec += conf.interval;
	
	if(conf.verbose)
		fprintf(stderr, "[%d] time control loop starting\n"
		       , cpu);
	
	ih=0;
	ii=0;
	
	for (i=0;;i++) {
		gettimeofday(&tv, NULL);
		diff = (tv.tv_sec - prev_tv.tv_sec)*1000000 +
			(tv.tv_usec - prev_tv.tv_usec);
		
		samples++;

		if(!calibrate) {
			hist[ih++] = diff;
			if(ih == histsize) {
				/* overflow: wrap */
				ih=0;
				overflows++;
				samples = 0;
			}
		}

		if( (tv.tv_sec >= sample_tv.tv_sec) &&
		    (tv.tv_usec >= sample_tv.tv_usec)) {
			if(!calibrate) {
				printf("%d:%d:%d:%d:%lld:%lld:%d:%d:\n",
				       cpu, ii, maxlat, minlat, 
				       average(hist, samples),
				       median(hist, samples),
				       samples, overflows);
				maxlat = MAXLAT;
				minlat = MINLAT;
				ih=0;
				samples = 0;
				overflows = 0;
			}
			if(!conf.average) {
				printf("%d:%d:%d:%d:%d:%d:%d:%d:\n",
				       cpu, ii, maxlat, minlat, 
				       0,
				       0,
				       samples, 0);
				maxlat = MAXLAT;
				minlat = MINLAT;
				samples = 0;
				overflows = 0;
			}

			if(conf.average && calibrate) {
				histsize = samples+samples/2;
				if(conf.verbose) printf("HISTSIZE = %d samples\n", histsize);
				hist = malloc(sizeof(int64_t)*histsize);
					      
				if(!hist) exit(2);
				calibrate = 0;
				samples = 0;
			}
			
			if(conf.pause) {
				struct timespec req;
				req.tv_sec = 0;
				req.tv_nsec = conf.pause;
				nanosleep(&req, NULL);
			}
			fflush(stdout);
			ii+=conf.interval;
			gettimeofday(&tv, NULL);
			sample_tv = tv;
			sample_tv.tv_sec += conf.interval;
		}

		prev_tv = tv;
		if (diff > maxlat) {
			maxlat = diff;
		}
		if (diff < minlat) {
			minlat = diff;
		}
	}
	return -1;
}

/* /sys/devices/system/cpu/online */
int cpu_online(const char *sysdir, int *nr_cpu)
{
	int fd, n;
	char fn[256];
	char buf[16], *p;

	*nr_cpu=0;
	
	snprintf(fn, sizeof(fn), "%s/devices/system/cpu/online", sysdir);
	
	fd = open(fn, O_RDONLY);
	if(fd == -1) return -1;
	n = read(fd, buf, sizeof(buf)-1);
	if(n < 1 ) return -1;
	
	close(fd);
	buf[n] = 0;
	p = strchr(buf, '-');
	if(!p) {
		/* single CPU system */
	  printf("single cpu system\n");
		*nr_cpu = 1;
		return 0;
	}
	
	*nr_cpu = atoi(p+1);
	(*nr_cpu)++;
	
	return 0;
}


int main(int argc, char **argv)
{
	int ncpu;
	int i, cpu, err;

	conf.interval = 2;

	if(jelopt(argv, 'h', "help", NULL, NULL)) {
		printf("schedlat [-hvim] [CPU#]\n"
		       " Version " VERSION " By Jens Låås, UU 2009-2011.\n"
		       " -v --verbose\n"
		       " -i --interval SECONDS [2]\n"
		       " -m --minmax (do not compute average and median)\n"
		       " -n --nice set priority [20, -20]\n"
		       " -a --average (compute average and median)\n"
		       " -p --pause NS -- nanosleep NS between intervals\n"
		       "\n"
		       "Output:\n"
		       "<cpu>:<secs>:<max>:<min>:<avg>:<median>:<samples>:<overflows>:\n"
		       "avg == -1 in case of overflow during calculation.\n"
		       "overflows > 0 if number of samples does not fit in calibrated array.\n"
		       "Values in usec.\n"
		       );
		exit(0);
	}

	if(jelopt(argv, 'v', "verbose", NULL, &err))
		conf.verbose = 1;
	if(jelopt(argv, 'a', "average", NULL, &err))
		conf.average = 1;
	if(jelopt(argv, 'm', "minmax", NULL, &err))
		conf.average = 0;
	if(jelopt_int(argv, 'i', "interval", &conf.interval, &err))
		;
	if(jelopt_int(argv, 'p', "pause", &conf.pause, &err))
		;
	if(jelopt_int(argv, 'n', "nice", &conf.prio, &err))
		;

	argc = jelopt_final(argv, &err);

	
	if(cpu_online("/sys", &ncpu)) {
		printf("Cannot read number of CPUs in system from \"/sys\"");
		exit(1);
	}
	
	if(conf.verbose)
		printf("Number of CPUS in system: %d\n", ncpu);

	if(setpriority(PRIO_PROCESS, getpid(), conf.prio)) {
		printf("Fail to set process priority to %d\n", conf.prio);
	}
	
	if(argc > 1) {
		cpu=atoi(argv[1]);
		cpumain(cpu, ncpu);
		exit(0);
	}
	
	for(i=0;i<ncpu;i++)
	{
		pid_t pid;
		
		pid = fork();
		if(pid == -1)
			exit(2);
		if(pid == 0)
			cpumain(i, ncpu);
	}
	while(1) sleep(10000);
}
