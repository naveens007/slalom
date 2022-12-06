#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include "slalom.h"

int errmsg(const char *msg);
int help_message();

/* Parameters used in controlling the slalom benchmark. */
struct slalom_parameters
{
    /* The problem size, expressed im seconds. */
    double goal;
    /* The lower bound. */
    int l_bound;
    /* Upper bound on problem size, used in iterating towards goal.  */
    int u_bound;
};

static struct slalom_parameters Params;

int validate (struct slalom_parameters params)
{
    if (params.goal <= 0 ||
            params.l_bound <=0 ||
            params.u_bound <= params.l_bound)
        return errmsg ("Invalid or incomplete parameters:\n\ttime, lower-bound, upper-bound");
    return 0;
}

int command_line(int argc, char **argv)
{
    int c;
    int digit_optind = 0;

    static struct option long_options[] =
    {
        {"time",        required_argument, 0, 't'},
        {"lower-bound", required_argument, 0, 'l'},
        {"upper-bound", required_argument, 0, 'u'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    /* No Arguments Specified. */
    if (argc == 1)
        return help_message();

    while (1)
        {
            int option_index = 0;
            c = getopt_long(argc, argv, "t:l:u:h", long_options, &option_index);

            if (c == -1)
                break;
#ifdef DEBUG
            printf ("%c : %s\n", c, optarg);
#endif
            switch (c)
                {
                case 't':
                    Params.goal = atof (optarg);
                    break;
                case 'l':
                    Params.l_bound = atoi (optarg);
                    break;
                case 'u':
                    Params.u_bound = atoi (optarg);
                    break;
                case 'h':
                    help_message ();
                    break;
                case '?':
                /* Error given by getopt already. */
                default:
                    return -1;
                }
        }
    if (optind < argc)
        return errmsg ("Extra or missed paramters.\n\tCheck -t, -l or -u values.");
    /* All options processed fine. */
    return validate (Params);
}

/**
 * The routine checks the lower bound provided by the user.
 * The requirement is that the time taken to solve the problem must
 * be less than the goal.
 */
int set_lower_bound(double goal, int lower_bound)
{
    // assert (lower_bound > 0);
    /* Elapsed time returned by Meter routine, in seconds. */
    double timing;
    /* In this case, number of FLOPs performed.            */
    double work;

    int ok = Meter(lower_bound, &timing, &work);
    if (timing >= goal)
        {
            fprintf(stderr, "Must take less than %g seconds. Took %g.\n", goal, timing);
            return -1;
        }
    return 0;
}
/**
 * The routine checks the upper bound provided by the user.
 * The requirement is that the time taken to solve the problem must
 * be greater than the goal.
 */
int set_upper_bound(double goal, int upper_bound)
{
    // assert (upper_bound > 0);
    /* Elapsed time returned by Meter routine, in seconds. */
    double timing;
    /* In this case, number of FLOPs performed.            */
    double work;
    int ok = Meter(upper_bound, &timing, &work);
    if (timing < goal)
        {
            fprintf(stderr, "Must take atleast %g seconds. Took %g.\n", goal, timing);
            return -1;
        }
    return 0;
}

int main(int argc, char **argv)
{
    int r_code = -1; /* return error by default. */
    if (command_line(argc, argv) != 0)
        return r_code;
    else if (set_lower_bound(Params.goal, Params.l_bound) != 0)
        return r_code;
    else if (set_upper_bound(Params.goal, Params.u_bound) != 0)
        return r_code;
    else
        return (slalom_main_loop(Params.goal, Params.l_bound, Params.u_bound));
}

int help_message()
{
    fprintf(stderr, "Usage:(All paramters below are required)\n");
    fprintf(stderr, "\tslalom -t <seconds> -l <integer> -u <integer>\n");
    fprintf(stderr, "\tslalom --time <seconds> --lower-bound integer --upper-bound integer \n");
    return -1;
}

int errmsg(const char *msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    return -1;
}

#include <string.h>
#include <unistd.h>
#include <libgen.h>

/**
 * Get the location of current working directory from where benchmark is invoked.
 *
 */
const char *get_path_name ()
{
    static char pathname[BUFSIZ] = {0};
    /* Check where is the binary running. Linux Specific. */
    readlink("/proc/self/exe", pathname, BUFSIZ);

    /* since we are returning static memory, no no need to worry
     * about the memory allocation by the caller.. */
    return pathname;
}

/**
 * The following is a wrapper for fopen () function declared in stdio.h.
 * It adds the absolute path of the binary where the executable resides.
 * This is needed because 'geom' file in the benchmark needs to be in the same directory
 * as binary 'slalom' is. Otherwise benchmark has to be run by changing the current
 * working directory to the binary location (that is cumbersome and non-intutive.)
 */

FILE *fopen_p(const char *infile, const char *mode)
{
    FILE *file_handle = 0;
    char dir_name[BUFSIZ] = {0};

    const char *pathname = get_path_name ();
    
    strcpy(dir_name, dirname ((char *) pathname));
    strcat(dir_name, "/");
    strcat(dir_name, infile);

    // printf ("Geometry at:%s\n", dir_name);
    file_handle = fopen(dir_name, mode);

    return file_handle;
}
