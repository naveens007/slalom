#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include "slalom.h"

/* Reads problem description from 'geom' file.        */
int Reader(double box[],      /* Out: Dimensions of box in x, y, z directions.  */
           double rho[][3],   /* Out: (RGB) Reflectivities of patches.          */
           double emiss[][3]); /* Out: (RGB) emissivities of patches.           */
/* Subdivides box faces into patches.                 */
int Region(int npatch,           /* In: Problem size.                             */
           int loop[][2],        /* Out: Patch number ranges for faces.           */
           double *box,          /* In: Dimensions of box in x, y, z directions.  */
           double place[][NMAX], /* Out: Width-height-depth positions of patches. */
           double size[][NMAX],  /* Out: Width-height sizes of patches.           */
           double *area);        /* Out: 8pi * areas of the patches.              */
/* Set up matrix to solve.                            */
int SetUp3(int npatch,           /* In: Problem size.                                 */
           int loop[][2],        /* In: Patch number ranges for faces.                */
           double area[],        /* In: 8 * pi * areas of the patches.                */
           double rho[][3],      /* In: (RGB) Reflectivities of the face interiors.   */
           double emiss[][3],    /* In: (RGB) Emissivities of the face interiors.     */
           double coeff[][NMAX], /* Out: The coefficients of the eqns to solve.       */
           double diag[][NMAX],  /* Out: (RGB) Diagonal terms of the system.          */
           double rhs[][NMAX]);  /* Out: (RGB) Right-hand sides of system to solve.   */
/* Write result to 'answer' file.                     */
int Storer(int npatch,            /* In: Problem size.                         */
           int loop[][2],         /* In: Patch number ranges for faces.        */
           double result[][NMAX], /* In: (RGB) Radiosity solutions.            */
           double place[][NMAX],  /* In: Width-height-depth positions of patches.      */
           double size[][NMAX]);  /* In: Width-height sizes of patches.                */
/* Verify the radiosity solution from solver.         */
int Verify(int npatch,             /* In: Problem size.                        */
           double coeff[][NMAX],   /* In: The coefficients of the eqns to solve.*/
           double diag[][NMAX],    /* In: (RGB) Diagonal terms of the system.   */
           double rhs[][NMAX],     /* In: (RGB) Right-hand sides of system to solve. */
           double result[][NMAX]); /* In: (RGB) Radiosity solutions.            */
/* Set up matrix to solve.                            */
void SetUp1(int npatch, int loop[][2],
            double coeff[][NMAX],
            double place[][NMAX],
            double size[][NMAX]);
/* Set up matrix to solve.                            */
void SetUp2(int npatch,           /* In: Problem size.                             */
            int loop[][2],        /* In: Patch number ranges for faces.            */
            double coeff[][NMAX], /* Out: The coefficients of the eqns to solve.   */
            double place[][NMAX], /* In: Width-height-depth positions of patches.  */
            double size[][NMAX]);  /* In: Width-height sizes of patches.            */
/* Solve the radiosity matrix.                        */
void Solver(int npatch,             /* In: Problem size.                              */
            int non0,               /* In: Index of first nonzero off-diagonal mat. elem. */
            double coeff[][NMAX],   /* In/Out: The coefficients of the eqns to solve. */
            double diag[][NMAX],    /* Out: (RGB) Diagonal terms of the system.       */
            double rhs[][NMAX],     /* In: (RGB) Right-hand sides of system to solve. */
            double result[][NMAX]); /* Out: (RGB) solution radiosities.               */
/* Wall clock in seconds.                             */
double When ();
/* Double dot product.                                */
double Ddot(register int n,     /* Number of elements in vectors.  */
            register double *a, /* Pointer to first vector.        */
            register int ia,    /* Stride of a vector in ELEMENTS. */
            register double *b, /* Pointer to second vector.       */
            register int ib);   /* Stride of b vector in ELEMENTS. */

int What (int, double, double);

int slalom_main_loop(
    double goal, /* User input, fixed-time benchmark goal, in seconds. */
    int lower_bound,
    int upper_bound /* Upper bound on problem size, used in iterating toward goal.  */
)
{
    int ok = 0;
    /* Avg between upper and lower bounds for bisection method.  */
    int mean = 0;

    /* Elapsed time returned by Meter routine, in seconds. */
    double timing = 0.0;
    /* In this case, number of FLOPs performed.          */
    double work = 0.0;
    /*
     *  While the [n, nupper] interval is larger than 1, bisect it and
     *  pick a half:
     */
    while (upper_bound - lower_bound > 1)
        {
            mean = (lower_bound + upper_bound) / 2;
            ok = Meter(mean, &timing, &work);
            if (timing < goal)
                lower_bound = mean;
            else
                upper_bound = mean;
            printf("New interval: [%d,%d]\n", lower_bound, upper_bound);
        }

    /* Ensure that most recent run was for lower_bound. */
    ok = Meter(lower_bound, &timing, &work);
    /* Print out final statistics. */
    What(lower_bound, timing, work);
    return ok;
}

/*****************************************************************************/
/* This routine should be edited to contain information for your system.     */
/*****************************************************************************/
int What(int n, double timing, double work)
{
    printf("\n");
#include "system_info.h"
    /* Get the system information and output it before actual performance stats.  */
    MachineInfo *info_ptr = get_system_info ();
    system_info (info_ptr);

    printf("\n");
    printf("M ops:    %-13lg        Time:       %-.3lf seconds\n",
           work * 1e-6, timing);
    printf("n:        %-6d               MFLOPS:     %-.5lg\n",
           n, (work / timing) * 1e-6);
    printf("Approximate data memory use: %d bytes.\n",
           8 * n * n + 120 * n + 800);
    return 0;
}

/*****************************************************************************/
/*  This routine measures time required on a revised LINPACK-type benchmark, */
/*  including input, matrix generation, solution, and output.                */
/*                                                                           */
/*  John Gustafson, Diane Rover, Michael Carter, and Stephen Elbert          */
/*  Ames Laboratory, 3/18/90                                                 */
/*                                                                           */
/*  Calls: Reader  Reads the problem description from secondary storage.     */
/*         Region  Partitions box surface into rectangular regions (patches).*/
/*         SetUp1  Sets up equations from patch geometries-parallel faces.   */
/*         SetUp2  Sets up equations from patch geometries-orthogonal faces. */
/*         SetUp3  Sets up equations-row normalization and radiant props.    */
/*         Solver  Solves the equations by LDL factorization.                */
/*         Storer  Stores solution (patch radiosities) on secondary storage. */
/*         When    Returns wall-clock time, in seconds.                      */
/*****************************************************************************/

int Meter(int npatch, double *timing, double *work)
/* int npatch;		 In, problem size, here the number of equations. */
/* double *timing,	 Out, number of seconds measured.                */
/*        *work;	 Out, work done, here the number of FLOPs.       */
{
    static double area[NMAX] = {0}, /* Areas of patches * 8 * pi.                */
    box[7] = {0},               /* Dimensions of box in x, y, z directions.  */
    coeff[NMAX][NMAX] = {0},    /* The coefficients of the eqns to solve.    */
    diag[3][NMAX] = {0},        /* Diag terms of the eqns to solve. (RGB)    */
    emiss[6][3] = {0},          /* (RGB) emissivities of patches.            */
    place[3][NMAX] = {0},       /* Width-height-depth position of patches.   */
    result[3][NMAX] = {0},      /* Answer radiosities (RGB).                 */
    rho[6][3] = {0},            /* (RGB) Reflectivities of patches.          */
    rhs[3][NMAX] = {0},         /* Right-hand sides of eqns to solve (RGB).  */
    size[2][NMAX] = {0};        /* Width-height sizes of patches.            */
    double ops[8] = {0},        /* Floating-point operation counts.          */
           p[6] = {0},          /* Number of patches in faces.               */
           sec[8] = {0},        /* Times for routines, in seconds.           */
           tmp1, tmp2;           /* Double temporary variables.               */
    int i,                    /* Loop counter.                             */
        itmp1,                /* Integer temporary variable.               */
        non0;                 /* Index of first nonzero off-diagonal elem. */
    static int loop[6][2] = {0};   /* Patch number ranges for faces.            */
    static const char *tasks[] =    /* Names of all the functions in benchmark.  */
    {
        "Reader", "Region",
        "SetUp1", "SetUp2",
        "SetUp3", "Solver",
        "Storer"
    };
    static const char *format = /* Output line format. */
        "%6.6s%8.3f%17.0f%14.6f%10.1f %%\n";

    /* First check that npatch lies between 6 and NMAX: */
    if (npatch < 6)
        {
            printf("Must be at least 6, the number of faces.\n");
            return (FALSE);
        }
    else if (npatch > NMAX)
        {
            printf("Exceeds %d = maximum for this system.\n", NMAX);
            return (FALSE);
        }

    /* Ensure that previous 'answer' file is deleted: */
    unlink("answer");

    /* Time the tasks, individually and collectively.  */
    sec[0] = When();
    if (!Reader(box, rho, emiss))
        return (FALSE);
    sec[1] = When();
    if (!Region(npatch, loop, box, place, size, area))
        return (FALSE);
    sec[2] = When();

    SetUp1(npatch, loop, coeff, place, size);
    sec[3] = When();
    SetUp2(npatch, loop, coeff, place, size);
    sec[4] = When();
    if (!SetUp3(npatch, loop, area, rho, emiss, coeff, diag, rhs))
        return (FALSE);
    sec[5] = When();
    non0 = loop[1][0];
    Solver(npatch, non0, coeff, diag, rhs, result);
    sec[6] = When();
    Storer(npatch, loop, place, size, result);
    sec[7] = When();
    *timing = sec[7] - sec[0];
    for (i = 0; i < 7; i++)
        sec[i] = sec[i + 1] - sec[i];

    /* Assess floating-point work done by each routine called, and total: */
    /* Note the ops counts are talleyed into a double array, and there    */
    /* some strange casts to double in some equations.  This is to        */
    /* prevent integer overflow.                                          */
    itmp1 = 0;
    tmp1 = 0.0;
    for (i = 0; i < 6; i++)
        {
            p[i] = loop[i][1] - loop[i][0] + 1;
            tmp1 += p[i] * p[i];
            itmp1 += sqrt(p[i] * box[i] / box[i + 1]) + 0.5;
        }
    tmp2 = p[0] * p[3] + p[1] * p[4] + p[2] * p[5];
    ops[0] = 258;
    ops[1] = 154 + (double)8 * itmp1 + npatch;
    ops[2] = 3 + 840 * tmp2;
    ops[3] = 430 * ((double)npatch * npatch - tmp1 - 2 * tmp2) / 2.0;
    ops[4] = 72 + (double)9 * npatch + (double)npatch * npatch - tmp1;
    ops[5] = npatch * (npatch * ((double)npatch + 7.5) - 2.5) - 21 + (non0 + 1) * ((non0 + 1) * (2 * ((double)non0 + 1) - 16.5) + 35.5) + (non0 + 1) * npatch * (9 - 3 * ((double)non0 + 1));
    ops[6] = 48 * npatch;
    *work = ops[0] + ops[1] + ops[2] + ops[3] + ops[4] + ops[5] + ops[6];

    /* Display timing-work-speed breakdown by routine. */
    printf("%d patches:\n", npatch);
    printf(" Task  Seconds       Operations        MFLOPS    %% of Time\n");
    for (i = 0; i < 7; i++)
        {
            if (sec[i] == 0.0)
                sec[i] = 0.001;
            printf(format, tasks[i], sec[i], ops[i], (ops[i] / sec[i]) * 1e-6,
                   100.0 * sec[i] / *timing);
        }
    printf(format, "TOTALS", *timing, *work, (*work / *timing) * 1e-6, 100.0);
    Verify(npatch, coeff, diag, rhs, result);

    return (TRUE);
}

/*****************************************************************************/
/*  This function should return the actual, wall clock time (not CPU time)   */
/*  in seconds as accurately as possible.  Change it to your system timer.   */
/*****************************************************************************/
double When()
{
    struct timeval tp;
    struct timezone tzp;
    gettimeofday(&tp, &tzp);
    return ((double)tp.tv_sec + (double)tp.tv_usec * 1e-6);
}

/*****************************************************************************/
/* The following routine reads in the problem description from secondary     */
/* storage, and checks that numbers are in reasonable ranges.                */
/*****************************************************************************/
int Reader(double box[],      /* Out: Dimensions of box in x, y, z directions.  */
           double rho[][3],   /* Out: (RGB) Reflectivities of patches.          */
           double emiss[][3]) /* Out: (RGB) emissivities of patches.            */
{
    /*
     *  Local variables:
     *    infile  Device number for input file.
     *    i, j    Loop counters.
     *    tmp1    Maximum emissivity, to check that emissivities are not all 0.
     */
    int i, j,      /* Loop counters.                            */
        n;         /* Number of args fscanf()'ed from file.     */
    double tmp1;   /* Maximum emissivity.                       */
    FILE *infile;  /* Input file pointer.                       */
    char buff[81]; /* Buffer used to eat a line of input.       */

    /* Open the input file and read in the data. */
    if ((infile = fopen("geom", "r")) == NULL)
        {
            printf("slalom:  'geom' geometry file not found.\n");
            exit(1);
        }

    /* Read the box coordinates and error check. */
    n = 0;
    for (i = 0; i < 3; i++)
        {
            n += fscanf(infile, "%lg", &box[i]);
        }
    fgets(buff, 80, infile); /* Eat the rest of the line. */
    if (n != 3)
        {
            printf("Must specify exactly 3 box coordinates.\n");
            exit(1);
        }

    /* Read the reflectivities and error check. */
    n = 0;
    for (j = 0; j < 3; j++)
        {
            for (i = 0; i < 6; i++)
                {
                    n += fscanf(infile, "%lg", &rho[i][j]);
                }
        }
    fgets(buff, 80, infile); /* Eat the rest of the line. */
    if (n != 18)
        {
            printf("Must specify exactly 18 box coordinates.\n");
            exit(1);
        }

    /* Read the emissivities and error check. */
    n = 0;
    for (j = 0; j < 3; j++)
        {
            for (i = 0; i < 6; i++)
                {
                    n += fscanf(infile, "%lg", &emiss[i][j]);
                }
        }
    fgets(buff, 80, infile); /* Eat the rest of the line. */
    if (n != 18)
        {
            printf("Must specify exactly 18 box coordinates.\n");
            exit(1);
        }
    fclose(infile);

    /* Now sanity-check the values that were just read. */
    for (j = 0; j < 3; j++)
        {
            if (box[j] < 1.0 || box[j] >= 100.0)
                {
                    printf("Box dimensions must be between 1 and 100.\n");
                    return (FALSE);
                }
            box[j + 3] = box[j];

            tmp1 = 0.0;
            for (i = 0; i < 6; i++)
                {
                    if (rho[i][j] < 0.000 || rho[i][j] > 0.999)
                        {
                            printf("Reflectivities must be between .000 and .999.\n");
                            return (FALSE);
                        }
                    if (emiss[i][j] < 0.0)
                        {
                            printf("Emissivity cannot be negative.\n");
                            return (FALSE);
                        }
                    if (tmp1 < emiss[i][j])
                        tmp1 = emiss[i][j];
                }
            if (tmp1 == 0.0)
                {
                    printf("Emissivities are zero.  Problem is trivial.\n");
                    return (FALSE);
                }
        }
    box[6] = box[3];
    return (TRUE);
}

/*****************************************************************************/
/* The following routine decomposes the surface of a variable-sized box      */
/* into patches that are as nearly equal in size and square as possible.     */
/*****************************************************************************/
int Region(int npatch,           /* In: Problem size.                             */
           int loop [][2],       /* Out: Patch number ranges for faces.           */
           double box [],        /* In: Dimensions of box in x, y, z directions.  */
           double place [][NMAX],/* Out: Width-height-depth positions of patches. */
           double size [][NMAX], /* Out: Width-height sizes of patches.           */
           double area [])       /* Out: 8pi * areas of the patches.              */
{
    int icol,      /* Loop counter over the number of columns. */
        ipatch,    /* Loop counter over the number of patches. */
        iface,     /* Loop counter over the number of faces.   */
        itmp1,     /* Integer temporary variables.             */
        itmp2,     /* Integer temporary variables.             */
        last,      /* Inner loop ending value.                 */
        lead,      /* Inner loop starting value.               */
        numcol,    /* Number of columns on faces.              */
        numpat,    /* Number of patches on a face.             */
        numrow;    /* Number of rows of patches in a column.   */
    double height, /* Height of a patch within a column.       */
           tmp1,      /* double temporary variables.              */
           tmp2,      /* double temporary variables.              */
           tmp3,      /* double temporary variables.              */
           tmp4,      /* double temporary variables.              */
           width;     /* Width of a column of patches.            */

    /* Allocate patches to each face, proportionate to area of each face. */
    tmp1 = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0]);
    tmp2 = 0.0;
    tmp3 = npatch;
    loop[0][0] = 0;
    for (iface = 0; iface < 5; iface++)
        {
            tmp2 = tmp2 + box[iface] * box[iface + 1];
            loop[iface][1] = (int)(tmp3 * tmp2 / tmp1 + 0.5) - 1;
            loop[iface + 1][0] = loop[iface][1] + 1;
        }
    loop[5][1] = npatch - 1;

    /* Subdivide each face into numpat patches. */
    for (iface = 0; iface < 6; iface++)
        {
            numpat = loop[iface][1] - loop[iface][0] + 1;
            tmp3 = 0.0;
            if (iface >= 3)
                tmp3 = box[iface - 1];
            numcol = (int)(sqrt(numpat * box[iface] / box[iface + 1]) + 0.5);
            if (numcol > numpat)
                numcol = numpat;
            if (numcol == 0)
                numcol = 1;
            width = box[iface] / numcol;
            itmp1 = numcol - 1;
            tmp1 = 0.0;
            for (icol = 0; icol < numcol; icol++)
                {
                    itmp2 = itmp1 / numcol;
                    numrow = (itmp1 + numpat) / numcol - itmp2;
                    if (numrow == 0)
                        {
                            printf("Eccentric box requires more patches.\n");
                            return (FALSE);
                        }
                    height = box[iface + 1] / numrow;
                    tmp2 = 0.0;
                    tmp4 = width * height * (8.0 * M_PI);
                    lead = loop[iface][0] + itmp2;
                    last = lead + numrow;
                    for (ipatch = lead; ipatch < last; ipatch++)
                        {
                            size[0][ipatch] = width;
                            size[1][ipatch] = height;
                            place[0][ipatch] = tmp1;
                            place[1][ipatch] = tmp2;
                            place[2][ipatch] = tmp3;
                            area[ipatch] = tmp4;
                            tmp2 = tmp2 + height;
                        }
                    tmp1 = tmp1 + width;
                    itmp1 = itmp1 + numpat;
                }
        }

    return (TRUE);
}

/*****************************************************************************/
/* This routine sets up the radiosity matrix for parallel patches.           */
/*****************************************************************************/
void SetUp1(int npatch,           /* In: Problem size.                             */
            int loop[][2],        /* In: Patch number ranges for faces.            */
            double coeff[][NMAX], /* Out: The coefficients of the eqns to solve.   */
            double place[][NMAX], /* In: Width-height-depth positions of patches.  */
            double size[][NMAX])  /* In: Width-height sizes of patches.            */
{
    int i, j, k,       /* General loop counters.                            */
        m, n,          /* General loop counters.                            */
        iface,         /* Loop counter over the number of faces.            */
        ipatch,        /* Loop counter over the number of patches.          */
        jface,         /* Face coupled to iface when computing mat. elems.  */
        jpatch;        /* Patch coupled to ipatch when computing mat. elems. */
    double d[2][2][2], /* Point-to-point couplings between patch corners.   */
           d2[2][2][2],   /* Squares of d values, to save recomputation.       */
           tmp1, tmp2,    /* Double temporary variables.                       */
           tmp3, tmp4,    /* Double temporary variables.                       */
           tmp5, tmp6,    /* Double temporary variables.                       */
           tmp7, tmp8;    /* Double temporary variables.                       */

    for (iface = 0; iface < 3; iface++)
        {
            jface = iface + 3;
            tmp1 = place[2][loop[jface][0]] * place[2][loop[jface][0]];
            for (ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++)
                {
                    for (jpatch = loop[jface][0]; jpatch <= loop[jface][1]; jpatch++)
                        {
                            for (j = 0; j < 2; j++)
                                {
                                    d[0][0][j] = place[j][jpatch] - place[j][ipatch];
                                    d[1][0][j] = d[0][0][j] + size[j][jpatch];
                                    d[0][1][j] = d[0][0][j] - size[j][ipatch];
                                    d[1][1][j] = d[1][0][j] - size[j][ipatch];
                                    d2[0][0][j] = d[0][0][j] * d[0][0][j];
                                    d2[1][0][j] = d[1][0][j] * d[1][0][j];
                                    d2[0][1][j] = d[0][1][j] * d[0][1][j];
                                    d2[1][1][j] = d[1][1][j] * d[1][1][j];
                                }
                            tmp2 = 0.0;
                            for (j = 0; j < 2; j++)
                                {
                                    for (m = 0; m < 2; m++)
                                        {
                                            for (i = 0; i < 2; i++)
                                                {
                                                    tmp3 = d2[m][i][1 - j] + tmp1;
                                                    tmp4 = sqrt(tmp3);
                                                    tmp5 = 1.0 / tmp4;
                                                    tmp8 = 0.0;
                                                    for (k = 0; k < 2; k++)
                                                        {
                                                            for (n = 0; n < 2; n++)
                                                                {
                                                                    tmp6 = d2[k][n][j];
                                                                    tmp7 = d[k][n][j];
                                                                    tmp2 =
                                                                        (tmp3 - tmp6) * log(tmp6 +
                                                                                            tmp3) -
                                                                        tmp2;
                                                                    tmp8 =
                                                                        -tmp7 * atan(tmp7 * tmp5) -
                                                                        tmp8;
                                                                }
                                                            tmp2 = -tmp2;
                                                            tmp8 = -tmp8;
                                                        }
                                                    tmp2 = -4.0 * tmp4 * tmp8 - tmp2;
                                                }
                                            tmp2 = -tmp2;
                                        }
                                }
                            coeff[ipatch][jpatch] = tmp2;
                            coeff[jpatch][ipatch] = tmp2;
                        }
                }
        }
}

/*****************************************************************************/
/* This routine sets up the radiosity matrix for orthogonal patches.         */
/*****************************************************************************/
void SetUp2(int npatch,           /* In: Problem size.                             */
            int loop[][2],        /* In: Patch number ranges for faces.            */
            double coeff[][NMAX], /* Out: The coefficients of the eqns to solve.   */
            double place[][NMAX], /* In: Width-height-depth positions of patches.  */
            double size[][NMAX])  /* In: Width-height sizes of patches.            */
{
    int i, j, k,    /* General loop counters.                            */
        m, n,       /* General loop counters.                            */
        iface,      /* Loop counter over the number of faces.            */
        ipatch,     /* Loop counter over the number of patches.          */
        jface,      /* Face coupled to iface when computing mat. elems.  */
        jpatch;     /* Patch coupled to ipatch when computing mat. elems. */
    double r[2][2], /* Point-to-point couplings between patch corners.   */
           r2[2][2],   /* Squares of r values, to save recomputation.       */
           s[2][2],    /* Point-to-point couplings between patch corners.   */
           tmp2,       /* Double temporary variables.                       */
           tmp3, tmp4, /* Double temporary variables.                       */
           tmp5, tmp6, /* Double temporary variables.                       */
           tmp7, tmp8; /* Double temporary variables.                       */

    for (iface = 0; iface < 6; iface++)
        {
            for (m = 0; m < 2; m++)
                {
                    jface = (iface + m + 1) % 6;
                    for (ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++)
                        {
                            for (jpatch = loop[jface][0]; jpatch <= loop[jface][1];
                                    jpatch++)
                                {
                                    r[0][0] = place[m][jpatch] - place[1 - m][ipatch];
                                    r[1][0] = r[0][0] + size[m][jpatch];
                                    r[0][1] = r[0][0] - size[1 - m][ipatch];
                                    r[1][1] = r[1][0] - size[1 - m][ipatch];
                                    s[0][0] = place[m][ipatch] - place[2][jpatch];
                                    s[1][0] = s[0][0] + size[m][ipatch];
                                    s[0][1] = place[1 - m][jpatch] - place[2][ipatch];
                                    s[1][1] = s[0][1] + size[1 - m][jpatch];
                                    for (i = 0; i < 2; i++)
                                        {
                                            for (j = 0; j < 2; j++)
                                                {
                                                    s[i][j] = s[i][j] * s[i][j];
                                                    r2[i][j] = r[i][j] * r[i][j];
                                                }
                                            /* Bump the term by a small real to avoid */
                                            /* singularities in coupling function:    */
                                            s[i][1] = s[i][1] + 1e-35;
                                        }
                                    tmp2 = 0.0;
                                    for (i = 0; i < 2; i++)
                                        {
                                            for (j = 0; j < 2; j++)
                                                {
                                                    tmp3 = s[i][0] + s[j][1];
                                                    tmp4 = sqrt(tmp3);
                                                    tmp5 = 1.0 / tmp4;
                                                    tmp8 = 0.0;
                                                    for (k = 0; k < 2; k++)
                                                        {
                                                            for (n = 0; n < 2; n++)
                                                                {
                                                                    tmp6 = r2[k][n];
                                                                    tmp7 = r[k][n];
                                                                    tmp2 =
                                                                        (tmp3 - tmp6) * log(tmp6 +
                                                                                            tmp3) -
                                                                        tmp2;
                                                                    tmp8 =
                                                                        -tmp7 * atan(tmp7 * tmp5) -
                                                                        tmp8;
                                                                }
                                                            tmp2 = -tmp2;
                                                            tmp8 = -tmp8;
                                                        }
                                                    tmp2 = -4.0 * tmp4 * tmp8 - tmp2;
                                                }
                                            tmp2 = -tmp2;
                                        }
                                    tmp2 = fabs(tmp2);
                                    coeff[ipatch][jpatch] = tmp2;
                                    coeff[jpatch][ipatch] = tmp2;
                                }
                        }
                }
        }
}

/*****************************************************************************/
/* This routine sets up the radiosity matrix... normalizes row sums to 1,    */
/* and includes terms derived from reflectivites and emissivities of faces.  */
/*****************************************************************************/
int SetUp3(int npatch,           /* In: Problem size.                                 */
           int loop[][2],        /* In: Patch number ranges for faces.                */
           double area[],        /* In: 8 * pi * areas of the patches.                */
           double rho[][3],      /* In: (RGB) Reflectivities of the face interiors.   */
           double emiss[][3],    /* In: (RGB) Emissivities of the face interiors.     */
           double coeff[][NMAX], /* Out: The coefficients of the eqns to solve.       */
           double diag[][NMAX],  /* Out: (RGB) Diagonal terms of the system.          */
           double rhs[][NMAX])   /* Out: (RGB) Right-hand sides of system to solve.   */
{

    /*
     *  Local variables:
     *    iface     Loop counter over the number of faces.
     *    ipatch    Outer loop counter over the number of patches.
     *    j         Loop counter over each color (R-G-B).
     *    jpatch    Inner loop counter over the number of patches.
     *    tmp1      double temporary variable.
     *    vtmp1-2   double vector temporary variables.
     */
    int j,        /* (RGB) Loop counter over each color.               */
        iface,    /* Loop counter over the number of faces.            */
        ipatch,   /* Outer loop counter over the number of patches.    */
        jpatch;   /* Inner loop counter over the number of patches.    */
    double tmp1,  /* Double temporary variable.                        */
           vtmp1[3], /* Double vector temporary variables.                */
           vtmp2[3]; /* Double vector temporary variables.                */

    /* Ensure that row sums to 1, and put in reflectivities (rho) and        */
    /* emissivities.                                                         */
    for (iface = 0; iface < 6; iface++)
        {
            for (j = 0; j < 3; j++)
                {
                    vtmp1[j] = 1.0 / rho[iface][j];
                    vtmp2[j] = emiss[iface][j] * vtmp1[j];
                }
            for (ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++)
                {
                    tmp1 = 0.0;
                    for (jpatch = 0; jpatch < loop[iface][0]; jpatch++)
                        {
                            tmp1 += coeff[ipatch][jpatch];
                        }
                    for (jpatch = loop[iface][1] + 1; jpatch < npatch; jpatch++)
                        {
                            tmp1 += coeff[ipatch][jpatch];
                        }
                    /* Make sure row sum (total form factor) is close to 1: */
                    if (fabs(tmp1 - area[ipatch]) > (0.5e-9 * tmp1))
                        {
                            printf("Total form factor is too far from unity.\n");
                            return (FALSE);
                        }
                    tmp1 = -tmp1;
                    /* Set coplanar patch interactions to zero. */
                    for (jpatch = loop[iface][0]; jpatch <= loop[iface][1]; jpatch++)
                        {
                            coeff[ipatch][jpatch] = 0.0;
                        }
                    /* Assign diagonal entries and right-hand sides. */
                    for (j = 0; j < 3; j++)
                        {
                            diag[j][ipatch] = vtmp1[j] * tmp1;
                            rhs[j][ipatch] = vtmp2[j] * tmp1;
                        }
                }
        }
    return (TRUE);
}

/*****************************************************************************/
/* This routine factors and backsolves a real, symmetric, near-dense matrix  */
/* by LDL factorization.  No pivoting; the matrix is diagonally dominant.    */
/*****************************************************************************/
void Solver(int npatch,            /* In: Problem size.                              */
            int non0,              /* In: Index of first nonzero off-diagonal mat. elem. */
            double coeff[][NMAX],  /* In/Out: The coefficients of the eqns to solve. */
            double diag[][NMAX],   /* Out: (RGB) Diagonal terms of the system.       */
            double rhs[][NMAX],    /* In: (RGB) Right-hand sides of system to solve. */
            double result[][NMAX]) /* Out: (RGB) solution radiosities.               */
{
    int i, j,    /* General loop counters.     */
        k, m;    /* General loop counters.     */
    double tmp1; /* Double temporary variable. */

    /* Load lower triangle of coefficients, diagonal, and solution vector. */
    for (m = 0; m < 3; m++)
        {
            for (i = non0; i < npatch; i++)
                {
                    coeff[i][i] = diag[m][i];
                    result[m][i] = rhs[m][i];
                    for (j = 0; j < i; j++)
                        {
                            coeff[i][j] = coeff[j][i];
                        }
                }

            /* Factor matrix, writing factors on top of original matrix. */
            for (j = 0; j < non0; j++)
                {
                    coeff[j][j] = 1.0 / diag[m][j];
                    result[m][j] = rhs[m][j];
                }

            for (j = non0; j < npatch; j++)
                {
                    for (k = non0; k < j; k++)
                        {
                            coeff[j][k] -= Ddot(k, &coeff[k][0], 1, &coeff[j][0], 1);
                        }
                    for (k = 0; k < j; k++)
                        {
                            tmp1 = coeff[j][k];
                            coeff[j][k] = tmp1 * coeff[k][k];
                            coeff[j][j] -= tmp1 * coeff[j][k];
                        }
                    coeff[j][j] = 1.0 / coeff[j][j];
                }

            /* Backsolve, in three stages (for L, D, and L transpose). */
            for (k = non0; k < npatch; k++)
                {
                    result[m][k] -= Ddot(k, &result[m][0], 1, &coeff[k][0], 1);
                }

            for (k = 0; k < npatch; k++)
                {
                    result[m][k] *= coeff[k][k];
                }

            for (k = npatch - 2; k >= non0; k--)
                {
                    result[m][k] -= Ddot(npatch - (k + 1), &result[m][k + 1], 1,
                                         &coeff[k + 1][k], NMAX);
                }

            for (k = non0 - 1; k >= 0; k--)
                {
                    result[m][k] -= Ddot(npatch - non0, &result[m][non0], 1,
                                         &coeff[non0][k], NMAX);
                }
        }
}

/*****************************************************************************/
/* The following routine writes the answer to secondary storage.             */
/*****************************************************************************/
int Storer(int npatch,            /* In: Problem size.                         */
           int loop[][2],         /* In: Patch number ranges for faces.        */
           double result[][NMAX], /* In: (RGB) Radiosity solutions.            */
           double place[][NMAX],  /* In: Width-height-depth positions of patches.      */
           double size[][NMAX])   /* In: Width-height sizes of patches.                */
{
    int iface,     /* Loop counter over number of faces.                */
        ipatch;    /* Loop counter of number of patches within a face.  */
    FILE *outfile; /* Output file pointer.                              */

    /* Write patch geometry to 'answer' file. */
    if ((outfile = fopen("answer", "w")) == NULL)
        {
            printf("Unable to open 'answer' file.\n");
            exit(1);
        }
    fprintf(outfile, "%d patches:\n", npatch);
    fprintf(outfile,
            " Patch  Face       Position in w, h, d              Width     Height\n");
    for (iface = 0; iface < 6; iface++)
        {
            for (ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++)
                {
                    fprintf(outfile,
                            "%5d   %4d%11.5lf%11.5lf%11.5lf  %11.5lf%11.5lf\n",
                            ipatch + 1, iface + 1,
                            place[0][ipatch],
                            place[1][ipatch],
                            place[2][ipatch], size[0][ipatch], size[1][ipatch]);
                }
        }
    /* Write patch radiosities to 'answer' file. */
    fprintf(outfile, "\n Patch  Face  Radiosities\n");
    for (iface = 0; iface < 6; iface++)
        {
            for (ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++)
                {
                    fprintf(outfile, "%5d   %4d%12.8lf%12.8lf%12.8lf\n",
                            ipatch + 1, iface + 1,
                            result[0][ipatch],
                            result[1][ipatch], result[2][ipatch]);
                }
        }
    fclose(outfile);
    return 0;
}

/*****************************************************************************/
/* This routine verifies that the computed radiosities satisfy the equations.*/
/*                                                                           */
/*  John Gustafson, Diane Rover, Michael Carter, and Stephen Elbert          */
/*  Ames Laboratory, 3/18/90                                                 */
/*****************************************************************************/
int Verify(int npatch,            /* In: Problem size.                        */
           double coeff[][NMAX],  /* In: The coefficients of the eqns to solve.*/
           double diag[][NMAX],   /* In: (RGB) Diagonal terms of the system.   */
           double rhs[][NMAX],    /* In: (RGB) Right-hand sides of system to solve. */
           double result[][NMAX]) /* In: (RGB) Radiosity solutions.            */
{
    double tmp1, tmp2; /* Double temporary variables. */
    double anorm,      /* Norm accumulation variable. */
           xnorm;         /* Norm accumulation variable. */
    int i, j, m;       /* General loop counters.      */

    tmp1 = 0.0;
    for (m = 0; m < 3; m++)
        {
            /* Copy lower triangle of coefficients to upper triangle, */
            /* and load diagonal.                                     */
            for (i = 0; i < npatch; i++)
                {
                    coeff[i][i] = diag[m][i];
                    for (j = 0; j < i; j++)
                        {
                            coeff[i][j] = coeff[j][i];
                        }
                }
            /* Multiply matrix by solution vector, and accum. norm of residual. */
            anorm = xnorm = 0.0;
            for (j = 0; j < npatch; j++)
                {
                    tmp2 = rhs[m][j];
                    for (i = 0; i < npatch; i++)
                        {
                            tmp2 -= (coeff[j][i] * result[m][i]);
                            anorm = MAX(anorm, fabs(coeff[j][i]));
                        }
                    xnorm = MAX(xnorm, fabs(result[m][j]));
                    tmp1 += fabs(tmp2);
                }
        }
    /* printf ("anorm = %g  xnorm = %g\n", anorm, xnorm); */
    tmp1 /= (anorm * xnorm);
    if (tmp1 > 3 * EPS)
        {
            printf("Residual is too large: %lg\n", tmp1);
            return (FALSE);
        }
    return (TRUE);
}

#ifdef SUN4

/*****************************************************************************/
/* Double precision dot product specifically written for Sun 4/370.          */
/* By Michael Carter and John Gustafson, May 30, 1990                        */
/* This code unrolls the dot product four ways since that's how many         */
/* registers are available on the SPARC.  Other RISC system will require     */
/* something very similar.  Also, unit stride is take advantage of in the    */
/* form of special cases.                                                    */
/*****************************************************************************/
double Ddot(register int n,     /* Number of elements in vectors.  */
            register double *a, /* Pointer to first vector.        */
            register int ia,    /* Stride of a vector in ELEMENTS. */
            register double *b, /* Pointer to second vector.       */
            register int ib)    /* Stride of b vector in ELEMENTS. */
{
    register double sum0 = 0.0, sum1 = 0.0, sum2 = 0.0, sum3 = 0.0;
    register int m = n & 3;
    size_t t; /* int t. Fix pointer to integer cast warnings.  */
    ;

    /* The ragged cleanup part. */
    while (m--)
        {
            sum0 += *a * *b;
            a += ia;
            b += ib;
        }

    /* The fast pipelined part */
    n >>= 2;
    if (ib == 1 && ia != 1)
        {
            t = ia;
            ia = ib;
            ib = t;
            t = (size_t)a;
            b = a;
            a = (double *)t;
        }

    /* We can optimize if one or more strides are equal to 1. */
    if (ia == 1)
        {
            /* This runs if both strides are 1. */
            if (ib == 1)
                {
                    ia <<= 2;
                    ib <<= 2;
                    while (n--)
                        {
                            sum0 += a[0] * b[0];
                            sum1 += a[1] * b[1];
                            sum2 += a[2] * b[2];
                            sum3 += a[3] * b[3];
                            a += ia;
                            b += ib;
                        }
                }
            /* This runs if stride of a only is equal to 1. */
            else
                {
                    ia <<= 2;
                    while (n--)
                        {
                            sum0 += a[0] * *b;
                            b += ib;
                            sum1 += a[1] * *b;
                            b += ib;
                            sum2 += a[2] * *b;
                            b += ib;
                            sum3 += a[3] * *b;
                            a += ia;
                            b += ib;
                        }
                }
        }
    /* This runs for the more general case.        */
    /* This is about .5 MFLOPS slower on Sun 4/370 */
    else
        {
            while (n--)
                {
                    sum0 += *a * *b;
                    a += ia;
                    b += ib;
                    sum1 += *a * *b;
                    a += ia;
                    b += ib;
                    sum2 += *a * *b;
                    a += ia;
                    b += ib;
                    sum3 += *a * *b;
                    a += ia;
                    b += ib;
                }
        }

    return (sum0 + sum1 + sum2 + sum3);
}

#else

/*****************************************************************************/
/* Generic double-precision dot product.  Unrolling will help pipelined      */
/* computers.  Modify accordingly.                                           */
/*****************************************************************************/
double Ddot(register int n,     /* Number of elements in vectors.  */
            register double *a, /* Pointer to first vector.        */
            register int ia,    /* Stride of a vector in ELEMENTS. */
            register double *b, /* Pointer to second vector.       */
            register int ib)    /* Stride of b vector in ELEMENTS. */
{
    register double sum = 0.0;

    while (n--)
        {
            sum += *a * *b;
            a += ia;
            b += ib;
        }
    return (sum);
}
#endif
