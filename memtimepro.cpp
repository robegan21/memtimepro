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

#define _USE_BSD

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <signal.h>

#include <errno.h>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/filestream.h"


#include "machdep.h"
#ifdef LINUX
#include "linux.h"
#endif
#ifdef sunos5
#include "sunos5.h"
#endif
#ifdef MACOSX
#include "macosx.h"
#endif

#define CAN_USE_RLIMIT_RSS
#define CAN_USE_RLIMIT_CPU
#define USLEEP_USECS 10000

int main (int argc, char *argv[] )
{
     struct rusage kid_usage;
     pid_t  kid;
     int    kid_status;
     int    exit_status = EXIT_SUCCESS;
     int    i, opt, echo_args = 0, exit_flag;
     long   sample_time=0, time = 0, track_time = 0;
     char   *jsonOutput = 0;
	 
     long maxkbytes=0; //kilobytes
     long maxseconds=0; //seconds
     long maxmillis=0;

     //unsigned long start, end;
     int max_samples = 80;

     memtime_info_tracker info_tracker(max_samples);

     if (argc < 2) {
	  char *tmp = strrchr(argv[0], '/');       
	  tmp = (tmp ? tmp + 1 : argv[0]);

	  fprintf(stderr, 
		  "%s: usage %s [ -o JSON_output_file ] [-t <interval>] [-e] [-m <maxkilobytes>] [-c <maxcpuseconds>] <cmd> [<params>]\n",
		  tmp,tmp);
	  exit(EXIT_FAILURE);
     }

     while ((opt = getopt(argc, argv, "+eo:t:m:c:")) != -1) {

	  switch (opt) {
          case 'o' :
               jsonOutput=optarg;
               break;

	  case 'e' : 
	       echo_args = 1;
	       break;

	  case 't' :
	       errno = 0;
	       sample_time = strtol(optarg, NULL, 0);
	       if (errno) {
		    perror("Illegal argument to t option");
		    exit(EXIT_FAILURE);
	       }
	       break;
	  case 'm' : 
	       errno = 0;
	       maxkbytes = atoi(optarg);
	       if (errno) {
		    perror("Illegal argument to m option");
		    exit(EXIT_FAILURE);
	       }
	       break;

	  case 'c' : 
	       errno = 0;
	       maxseconds = atoi(optarg);
	       if (errno) {
		    perror("Illegal argument to c option");
		    exit(EXIT_FAILURE);
	       }
		  maxmillis=1000*maxseconds;
	       break;

	  }
     }

     if (echo_args) {
	  fprintf(stderr,"Command line: ");
	  for (i = optind; i < argc; i++)
	       fprintf(stderr,"%s ", argv[i]);
	  fprintf(stderr,"\n");
     }

     struct memtime_info info, final_info;
     info_tracker.track(final_info); // start the timer

     memtime_limit lim;
     memtime_fork f;
    
     switch (kid = f.native_fork()) {
	
     case -1 :
	  perror("fork failed");
	  exit(EXIT_FAILURE);
	
     case 0 :	
#if defined(CAN_USE_RLIMIT_RSS)	  
	  if (maxkbytes>0) {
	       lim.set_mem_limit((long)maxkbytes*1024);
	  }
#endif
#if defined(CAN_USE_RLIMIT_CPU)	  
	  if (maxseconds>0) {
	       lim.set_cpu_limit((long)maxseconds);
	  }
#endif
	  execvp(argv[optind], &(argv[optind]));
	  perror("exec failed");
	  exit(EXIT_FAILURE);
	
     default :
	  break;
     }

     process_tracker tracker(kid);
     try {

     do {

	  if ((track_time++ & 0x3f) == 0) {
	    info = tracker.get_sample();
            info_tracker.track(info);

	     if (sample_time) {
	       time++;
	       if (time == 10 * sample_time) {
		    
		    fprintf(stderr,"%.2f user, %.2f system, %.2f elapsed"
			    " -- VSize = %ldKB, RSS = %ldKB\n",
			    (double)info.utime_ms/1000.0,
			    (double)info.stime_ms/1000.0,
			    (double)(info_tracker.get_walltime_ms())/1000.0,
			    info.vsize_kb, info.rss_kb);
		    fflush(stdout);
		    
		    time = 1;
	       }
            }
#if !defined(CAN_USE_RLIMIT_RSS)	  
	    if ((maxkbytes > 0) && (info.vsize_kb > maxkbytes)) {
	  	kill(kid,SIGTERM);
	    }
#endif
#if !defined(CAN_USE_RLIMIT_CPU)	  
	    if ((maxmillis > 0) && (info.utime_ms > maxmillis)) {
	  	kill(kid,SIGTERM);
	    }
#endif	  
	  }

	  if (usleep(USLEEP_USECS) != 0)
              fprintf(stderr, "Could not usleep(USLEEP_USECS)\n");

	  exit_flag = ((wait4(kid, &kid_status, WNOHANG, &kid_usage) == kid)
		       && (WIFEXITED(kid_status) || WIFSIGNALED(kid_status)));

     } while (!exit_flag);
     
     info_tracker.set_end(final_info);
     
     if (WIFEXITED(kid_status)) {
	  fprintf(stderr, "Exit [%d]\n", WEXITSTATUS(kid_status));
          exit_status += WEXITSTATUS(kid_status);
     } else {
	  fprintf(stderr, "Killed [%d]\n", WTERMSIG(kid_status));
          exit_status += WTERMSIG(kid_status);
     }

     {
	  double kid_utime = ((double)kid_usage.ru_utime.tv_sec 
			      + (double)kid_usage.ru_utime.tv_usec / 1E6);
	  double kid_stime = ((double)kid_usage.ru_stime.tv_sec 
			      + (double)kid_usage.ru_stime.tv_usec / 1E6);

          info_tracker.set_times(kid_utime * 1000.0, kid_stime * 1000.0);

	  fprintf(stderr, "%.2f user, %.2f system, %.2f elapsed -- "
		  "Max VSize = %ldKB, Max RSS = %ldKB\n", 
		  kid_utime, kid_stime, (double)(final_info.get_walltime_ms()) / 1000.0,
		  info_tracker.get_max_vmem(), info_tracker.get_max_rss());

          if (jsonOutput != 0) {

	  rapidjson::Document report;
	  if (report.Parse<0>("{}").HasParseError() != 0)
	    printf("Error!\n");
	    assert(report.IsObject());
            {
                char *realPath, tmpPath[4096];
                realPath = realpath(argv[optind], tmpPath);
                if (realPath == NULL) {
                    report.AddMember("program", argv[optind], report.GetAllocator());
                } else {
	            report.AddMember("program", realPath, report.GetAllocator());
                    //free(realPath);   
                }
            }
	  
	    rapidjson::Value args;
	    args.SetArray();
	    args.Reserve(argc-optind, report.GetAllocator());
	    for(int i=optind+1 ; i < argc ; i++) {
	      args.PushBack(argv[i], report.GetAllocator());
	    }
	    report.AddMember("arguments", args, report.GetAllocator());
	    report.AddMember("exit_status", exit_status, report.GetAllocator());
            memtime_info final_usage = info_tracker.get_final_info();

	    //report.AddMember("utime", kid_utime, report.GetAllocator());
	    //report.AddMember("stime", kid_stime, report.GetAllocator());
	    //report.AddMember("max_vmem_kb", (int64_t) info_tracker.get_max_vmem(), report.GetAllocator());
	    //report.AddMember("max_rss_kb", (int64_t) info_tracker.get_max_rss(), report.GetAllocator());
	    report.AddMember("start_time", (int64_t) final_info.start_ms/1000, report.GetAllocator());
	    report.AddMember("wall_time", (double) (final_info.end_ms - final_info.start_ms)/1000.0, report.GetAllocator());

            info_tracker.get_final_info().getJSON("usage", report);

            FILE *fp = fopen(jsonOutput, "w");
            if (fp != 0) {
	      rapidjson::FileStream f(fp);
              rapidjson::PrettyWriter<rapidjson::FileStream> writer(f);
              report.Accept(writer);        // Accept() traverses the DOM and generates Handler events.
              fclose(fp);
           }

          }

     }

     } catch (...) {
          perror("Abnormal termination of memtime\n");
     }

     exit(exit_status);
}

