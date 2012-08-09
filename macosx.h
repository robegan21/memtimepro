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

#ifndef _MAC_OS_X_H_
#define _MAC_OS_X_H_

#include <sys/time.h>
//#include <asm/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>

#include <sys/resource.h>

#include "machdep.h"
#include "macosx-fork.h"
#include "macosx-task.h"

class process_tracker : public process_tracker_base {
private:
     task_t child_task;

public:
     process_tracker(pid_t process_id) :
          process_tracker_base(process_id) {
          child_task = memtime_fork::get_child_task();
     }
     virtual ~process_tracker() {}

memtime_info get_sample()
{
     memtime_info info;
     // TODO implement for Mac
     int ret = getmem_v1(child_task, &info.rss_kb, &info.vsize_kb, &info.utime_ms, &info.stime_ms);     
     if (ret != 0)
       fprintf(stderr, "Warning getmem_v1 failed!\n");
     info.rss_kb /= 1024;
     info.vsize_kb /= 1024;
     //fprintf(stderr, "%d - %d / %d: %ld kb, %ld kb, %ld ms, %ld ms\n", ret, get_process_id(), child_task, info.vsize_kb, info.rss_kb, info.utime_ms, info.stime_ms);
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


#endif /* _MAC_OS_X_H_ */

