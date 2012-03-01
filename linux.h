/* 
 *---------------------------------------------------------------------------*
 *
 * Copyright (c) 2000, Johan Bengtsson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *---------------------------------------------------------------------------*/

#ifndef _LINUX_H_
#define _LINUX_H_

#include <sys/time.h>
#include <asm/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>

#include <sys/resource.h>

#include "machdep.h"

class process_tracker : public process_tracker_base {
private:
     int proc_fd;

public:
     process_tracker(pid_t process_id) :
          process_tracker_base(process_id), proc_fd(-1) {
     }
     virtual ~process_tracker() {}



int init_machdep(pid_t process)
{
     char filename[64];
     sprintf(filename, "/proc/%d/stat", (int)process);
     proc_fd = open(filename, O_RDONLY);

     return (proc_fd != -1) ? 0 : -1;
}

void destroy_machdep()
{
     close(proc_fd);
}

memtime_info get_sample()
{
     static char buffer[2048];
     char *tmp;
     long i, utime, stime;
     unsigned long vsize, rss;
     int rc;
     memtime_info info;

     lseek(proc_fd, 0, SEEK_SET);
     rc = read(proc_fd, buffer, 2048);

     if (rc == -1)
	  return info;

     *(buffer + rc) = '\0';

     for (i=0, tmp=buffer; i < 13; i++)
	  tmp = strchr(tmp + 1, ' ');

     sscanf(tmp + 1, "%ld %ld", &utime, &stime);
    
     for (/* empty */; i < 22; i++)
	  tmp = strchr(tmp + 1, ' ');

     sscanf(tmp + 1, "%lu %lu", &vsize, &rss);

     info.utime_ms = utime * 1000;
     info.stime_ms = stime * 1000;

     info.rss_kb = (rss * getpagesize()) / 1024;
     info.vsize_kb = vsize / 1024;

     return info;
}

};

class memtime_limit : public memtime_limit_base {
public:

int set_mem_limit(long maxbytes)
{
	struct  rlimit rl;
	long int softlimit=(long int)maxbytes*0.95;
	rl.rlim_cur=softlimit; 
	rl.rlim_max=maxbytes;
	return setrlimit(RLIMIT_AS,&rl);
}

int set_cpu_limit(long maxseconds)
{
	struct  rlimit rl;
	rl.rlim_cur=maxseconds; 
	rl.rlim_max=maxseconds;
	return setrlimit(RLIMIT_CPU,&rl);
}

};

class memtime_info_tracker : public memtime_info_tracker_base {
public:
    memtime_info_tracker(int max_samples) : memtime_info_tracker_base(max_samples) {}

unsigned long get_time()
{
     struct timeval now;
     struct timezone dummy;
     int rc;

     rc = gettimeofday(&now, &dummy);
    
     if (rc == -1) {
	  return 0;
     }

     return (now.tv_sec * 1000) + (now.tv_usec / 1000);
}
};

class memtime_fork : public memtime_fork_base {
};

#endif /* _LINUX_H_ */

