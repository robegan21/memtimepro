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

#ifndef MACHDEP_H
#define MACHDEP_H

#include<vector>

class memtime_info {
public:
     unsigned long utime_ms;
     unsigned long stime_ms;
     unsigned long rss_kb;
     unsigned long vsize_kb;
     unsigned long start_ms;
     unsigned long end_ms;
     unsigned long num_samples;
     memtime_info() {
         reset();
     }
     memtime_info(const memtime_info &copy) {
         *this = copy;
     }
     memtime_info &operator=(const memtime_info &copy) {
         utime_ms = copy.utime_ms;
         stime_ms = copy.stime_ms;
         rss_kb = copy.rss_kb;
         vsize_kb = copy.vsize_kb;
         start_ms = copy.start_ms;
         end_ms = copy.end_ms;
         num_samples = copy.num_samples;
         return *this;
     }
     memtime_info &operator+(const memtime_info &copy) {
         utime_ms += copy.utime_ms;
         stime_ms += copy.stime_ms;
         rss_kb = rss_kb > copy.rss_kb ? rss_kb : copy.rss_kb;
         vsize_kb = vsize_kb > copy.vsize_kb ? vsize_kb : copy.vsize_kb;
         start_ms = start_ms < copy.start_ms ? start_ms : copy.start_ms;
         end_ms = end_ms > copy.end_ms ? end_ms : copy.end_ms;
         num_samples += copy.num_samples;
         return *this;
     }
     void reset() {
         utime_ms = stime_ms = rss_kb = vsize_kb = start_ms = end_ms = num_samples = 0;
     }
     void reset( unsigned long time_ms) {
         reset();
         start_ms = time_ms;
     }
     void set_end_time(unsigned long time_ms) {
         end_ms = time_ms;
         num_samples = 1;
     }
};

class memtime_info_tracker_base {
public:
     typedef std::vector<memtime_info> SampleVector;
     typedef SampleVector::iterator SampleVectorI;
     typedef SampleVector::const_iterator SampleVectorIc;

     memtime_info_tracker_base(int max_samples) : _max_samples(max_samples), _current(), _samples_per_bin(1) {
          _samples.reserve(_max_samples);
          _current.reset(get_time());
     }
     void track(memtime_info &data) {

          if (_current.num_samples < _samples_per_bin) {
              data.set_end_time(get_time());
              _current = _current + data;
          }
          if (_current.num_samples >= _samples_per_bin) {
              if (_samples.size() == _max_samples)
                   collapse();
              _samples.push_back( _current );
              _current.reset(_current.end_ms);
          }
     }
     virtual unsigned long get_time() {return 0;}
     long get_max_vmem() const {
         long max_vmem = 0;
         for(SampleVectorIc it = _samples.begin(); it != _samples.end(); it++)
             max_vmem = max_vmem >= (long) it->vsize_kb ? max_vmem : it->vsize_kb;
         return max_vmem;
     }
     long get_max_rss() const {
         long max_rss = 0;
         for(SampleVectorIc it = _samples.begin(); it != _samples.end(); it++)
             max_rss = max_rss >= (long) it->rss_kb ? max_rss : it->rss_kb;
         return max_rss;
     }


private:
     unsigned long _max_samples;
     SampleVector _samples;
     memtime_info _current;
     unsigned long _samples_per_bin;

     void collapse() {
         unsigned long j = 0;
         for(unsigned long i = 0; i < _samples.size() ; i++) {
             j = i / 2;
             if (j != i) {
               _samples[j] = _samples[j] + _samples[i];
               _samples[i].reset();
             }
         }
         _samples.resize(j+1);
         _samples_per_bin *= 2;
     }
};

class process_tracker_base {
public:
     process_tracker_base(pid_t process_id) :
          _process_id(process_id) {
          if (!this->init_machdep(_process_id)) {
              perror("Count not initialize process tracker!\n");
              throw;
          }
     }
     virtual ~process_tracker_base() {
          destroy_machdep();
     }
     pid_t get_process_id() {
          return _process_id;
     }

private:
     pid_t _process_id;

public:
     virtual int init_machdep(pid_t process_id) { return -1; };
     virtual void destroy_machdep() {};
     virtual memtime_info get_sample() = 0;
};

class memtime_limit_base {
public:
     virtual int set_mem_limit(long maxbytes) { return -1; }
     virtual int set_cpu_limit(long maxseconds) { return -1; }
};

class memtime_fork_base {
public:

virtual pid_t native_fork ()
{
     return fork();
}

};

#endif /* MACHDEP_H */
