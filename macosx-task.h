#include <mach/task.h>
#include <mach/mach_init.h>
#include <sys/time.h>
#include <sys/resource.h>

int getmem_v1(int task_id, unsigned long *rss, unsigned long *vs, unsigned long *user, unsigned long *system)
{
    //task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(task_id,
       TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count))
    {
        fprintf(stderr, "Could not get task_info on %d\n", task_id);
        return -1;
    }
    *rss = t_info.resident_size;
    *vs  = t_info.virtual_size;
    //*user = t_info.user_time.seconds * 1000000 + t_info.user_time.microseconds;
    //*system = t_info.system_time.seconds * 1000000 + t_info.system_time.microseconds;
    struct rusage r_usage;
    if (getrusage(RUSAGE_CHILDREN, &r_usage) != 0) {
        fprintf(stderr, "Could not getrusage!\n");
        return -1;
    }
    *user   = r_usage.ru_utime.tv_sec * 1000 + r_usage.ru_utime.tv_usec / 1000;
    *system = r_usage.ru_stime.tv_sec * 1000 + r_usage.ru_stime.tv_usec / 1000;
    return 0;
}
