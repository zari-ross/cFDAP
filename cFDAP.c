/*******************************************/
/* cFDAP.c (c) 2015 Maxim Igaev, Osnabrück */
/*******************************************/

/* Compiling with gsl and blas
   cc/gcc cFDAP.c -o cFDAP -lgsl -lgslcblas -lm */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h> 
#include <string.h>
#include <math.h>
#include <complex.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>

/* Global variables */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Default values */
#define DEFAULT_DF 11.0 /* Diffusion constant */
#define DEFAULT_R 3.0 /* The half length of the AR */
#define DEFAULT_T_INI 0.0 /* Time range [T_INI, T_END] */
#define DEFAULT_T_END 112.0
#define DEFAULT_N 113 /* Number of points in the curves */
#define DEFAULT_KON_INIT 0.5 /* Starting value for kon */
#define DEFAULT_KOFF_INIT 0.5 /* Starting value for koff */

/* MACROS */
#define NELEMS_1D(x) (sizeof(x)/sizeof((x)[0]))
#define NROW_2D(x) NELEMS_1D(x)
#define NCOL_2D(x) ((sizeof(x)/sizeof((x)[0][0]))/(sizeof(x)/sizeof((x)[0])))
#define NELEMS_2D(x) (sizeof(x)/sizeof((x)[0][0]))

/* STRUCTURES */
struct data {
    size_t n;
    double Df;
    double R;
    double * time;
    double * y;
    double * sigma;
    char * m;
};

/* FUNCTION DECLARATIONS */
double complex fullModel(double complex s, double kon,
                         double koff, double Df, double R);
double complex fullModel_kon(double complex s, double kon,
                             double koff, double Df, double R);
double complex fullModel_koff(double complex s, double kon,
                              double koff, double Df, double R);
double complex effectiveDiffusion(double complex s, double x,
                                  double Df, double R);
double complex effectiveDiffusion_x(double complex s, double x,
                                    double Df, double R);
double complex reactionDominantPure(double complex s, double kon,
                                    double koff, double Df, double R);
double complex reactionDominantPure_kon(double complex s, double kon,
                                        double koff, double Df, double R);
double complex reactionDominantPure_koff(double complex s, double kon,
                                         double koff, double Df, double R);
double complex hybridModel(double complex s, double kon,
                           double koff, double Df, double R);
double complex hybridModel_kon(double complex s, double kon,
                               double koff, double Df, double R);
double complex hybridModel_koff(double complex s, double kon,
                                double koff, double Df, double R);
double invlap(double t, double kon, double koff,
              double Df, double R, char *m, int functionOrDerivative);
int model_f(const gsl_vector * x, void *data, gsl_vector * f);
int model_df(const gsl_vector * x, void *data, gsl_matrix * J);
int model_fdf (const gsl_vector * x, void *data, gsl_vector * f, gsl_matrix * J);
void print_state (size_t iter, gsl_multifit_fdfsolver * s);
void bad_input(void);

/* FUNCTIONS */

/* Laplace images of FDAP(t). Note: These function must take
   a complex argument s and return a complex value */
double complex
fullModel(double complex s, double kon, double koff, double Df, double R) {
    return (1.0/(1.0 + kon/koff)) * (1.0 + kon/(s + koff)) * (1.0/s - 1.0/2.0/s/csqrt(R*R*s*(1.0 + kon/(s + koff))/Df) * (1.0 - cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)))) + (kon/koff/(1.0 + kon/koff))/(s + koff);
}

double complex
fullModel_kon(double complex s, double kon, double koff, double Df, double R) {
    return -koff*(-kon - koff + koff*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + kon*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 2.0*koff*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 2.0*kon*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) - 2.0*s + 2.0*s*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)))/4.0/s/(s + koff)/(kon + koff)/(kon + koff)/csqrt(R*R*s*(1.0 + kon/(s + koff))/Df);
}

double complex
fullModel_koff(double complex s, double kon, double koff, double Df, double R) {
    return kon*(-cpow(koff, 2.0) - 2.0*cpow(s, 2.0) - 4.0*s*koff - kon*koff + kon*koff*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 2.0*koff*koff*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 2.0*s*s*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 4.0*s*koff*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + koff*koff*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 2.0*s*kon*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) + 2.0*kon*koff*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)*cexp(-2.0*csqrt(R*R*s*(1.0 + kon/(s + koff))/Df)) - 2*s*kon)/4.0/s/(s + koff)/(s + koff)/(kon + koff)/(kon + koff)/csqrt(R*R*s*(1.0+kon/(s + koff))/Df);
}

double complex
effectiveDiffusion(double complex s, double x, double Df, double R) {
    /* x = kon/koff */
    return 1.0/s - 1.0/2.0/s/csqrt(R*R*s*(1.0 + x)/Df)*(1.0 - cexp(-2.0*csqrt(R*R*s*(1.0 + x)/Df)));
}

double complex
effectiveDiffusion_x(double complex s, double x, double Df, double R) {
    /* x = kon/koff */
    return (1.0 - (1.0 + 2.0*csqrt(s*R*R/Df*(1.0 + x)))*cexp(-2.0*csqrt(s*R*R/Df*(1.0 + x))))/(4.0*s*(1.0 + x)*csqrt(s*R*R/Df*(1.0 + x)));
}

double complex
reactionDominantPure(double complex s, double kon, double koff, double Df, double R) {
    return koff/(kon + koff)*(1.0/s - 1.0/2.0/s/csqrt(R*R*s/Df)*(1.0 - cexp(-2.0*csqrt(R*R*s/Df)))) + kon/(kon + koff)/(s + koff);
}

double complex
reactionDominantPure_kon(double complex s, double kon, double koff, double Df, double R) {
    return (1.0 - kon/(kon + koff))/(kon + koff)/(s + koff) - koff*(1.0/s - 1.0/2.0/s/csqrt(R*R*s/Df)*(1.0 - cexp(-2.0*csqrt(R*R*s/Df))))/cpow(kon + koff, 2.0);
}

double complex
reactionDominantPure_koff(double complex s, double kon, double koff, double Df, double R) {
    return -kon/(kon + koff)/(s + koff)*(1.0/(kon + koff) + 1.0/(s + koff)) + (1.0/s - 1.0/2.0/s/csqrt(R*R*s/Df)*(1.0 - cexp(-2.0*csqrt(R*R*s/Df))))/(kon + koff)*(1.0 - koff/(kon + koff));
}

double complex
hybridModel(double complex s, double kon, double koff, double Df, double R) {
    return (koff/(s + koff))*(1.0/s - 1.0/2.0/s/csqrt(R*R*kon*s/Df/(s + koff))*(1.0 - cexp(-2.0*csqrt(R*R*kon*s/Df/(s + koff))))) + 1.0/(s + koff);
}

double complex
hybridModel_kon(double complex s, double kon, double koff, double Df, double R) {
    return koff/s/(s + koff)*(s*R*R/Df*(1.0 - cexp(-2.0*csqrt(R*R*s/Df*kon/(s + koff))))/4.0/(s + koff)/cpow(R*R*s/Df*kon/(s + koff), 3.0/2.0) - cexp(-2.0*csqrt(R*R*s/Df*kon/(s + koff)))/2.0/kon);
}

double complex
hybridModel_koff(double complex s, double kon, double koff, double Df, double R) {
    return -cexp(-2.0*csqrt(R*R*s/Df*kon/(s + koff)))/(4.0*s*cpow(s + koff, 3)*cpow(R*R*s/Df*kon/(s + koff), 3.0/2.0))*(R*R*s/Df*kon*(koff + 2.0*s)*cexp(2.0*csqrt(R*R*s/Df*kon/(s + koff))) - 2.0*koff*(s + koff)*cpow(R*R*s/Df*kon/(s + koff), 3.0/2.0) - R*R*s/Df*kon*koff - 2.0*kon*R*R*cpow(s, 2.0)/Df);
}

/* Function invlap(t, kon, koff) numerically inverts a Laplace
   image function F(s) into f(t) using the Fast Fourier Transform
   (FFT) algorithm for a specific time moment "t", an upper
   frequency limit "omega", a real parameter "sigma" and the
   number of integration intervals "n_int".
   
   Recommended values: omega > 100, n_int = 50*omega
   Default values:     omega = 200, n_int = 10000
   
   Sigma is a real number which must be a little bit bigger than
   the real part of the rigthmost pole of the function F(s).
   For example, if the rightmost pole is s = 2.0, then sigma could
   be equal to 2.05. This is done to keep all poles at the left of
   the integration area on the complex plane.
  
   Creator:  Fausto Arinos de Almeida Barbuto (Calgary, Canada)
   Date: May 18, 2002
   E-mail: fausto_barbuto@yahoo.ca
  
   Algorithm:
   Huddleston, T. and Byrne, P: "Numerical Inversion of
   Laplace Transforms", University of South Alabama, April
   1999 (found at http://www.eng.usouthal.edu/huddleston/
   SoftwareSupport/Download/Inversion99.doc)
  
   Modified by Maxim Igaev, 2015 */
double
invlap(double t, double kon, double koff, double Df, double R, char *m, int functionOrDerivative) {
    /* defining constants */
    int i, n_int = 10000;
    double omega = 200.0, sigma = 0.05, delta = omega/((double) n_int);
    double sum = 0.0, wi = 0.0, wf, fi, ff;
    double complex witi, wfti; 

    /* loading one of the laplace image functions */
    double complex (*laplace_fun)();
    if (strcmp(m, "fullModel") == 0) {
        if (functionOrDerivative == 0) {
            laplace_fun = &fullModel;
        }
        else if (functionOrDerivative == 1) {
            laplace_fun = &fullModel_kon;
        }
        else if (functionOrDerivative == 2) {
            laplace_fun = &fullModel_koff;
        }
        else {
            fprintf(stderr, "ERROR: invlap takes only values 0, 1 or 2 for the model and derivatives, respectively.");
        }
    }
    else if (strcmp(m, "effectiveDiffusion") == 0) {
        if (functionOrDerivative == 0) {
            laplace_fun = &effectiveDiffusion;
        }
        else if (functionOrDerivative == 1) {
            laplace_fun = &effectiveDiffusion_x;
        }
        else {
            fprintf(stderr, "ERROR: effectiveDiffusion model has only one fit parameter x.");
        }
    }
    else if (strcmp(m, "hybridModel") == 0) {
        if (functionOrDerivative == 0) {
            laplace_fun = &hybridModel;
        }
        else if (functionOrDerivative == 1) {
            laplace_fun = &hybridModel_kon;
        }
        else if (functionOrDerivative == 2) {
            laplace_fun = &hybridModel_koff;
        }
        else {
            fprintf(stderr, "ERROR: invlap takes only values 0, 1 or 2 for the model and derivatives, respectively.");
        }
    }
    else if (strcmp(m, "reactionDominantPure") == 0) {
        if (functionOrDerivative == 0) {
            laplace_fun = &reactionDominantPure;
        }
        else if (functionOrDerivative == 1) {
            laplace_fun = &reactionDominantPure_kon;
        }
        else if (functionOrDerivative == 2) {
            laplace_fun = &reactionDominantPure_koff;
        }
        else {
            fprintf(stderr, "ERROR: invlap takes only values 0, 1 or 2 for the model and derivatives, respectively.");
        }
    }

    for(i = 0; i < n_int; i++) {
        witi = 0.0 + (wi*t)*I;

        wf = wi + delta;
        wfti = 0.0 + (wf*t)*I;

        fi = creal(cexp(witi)*laplace_fun(sigma + wi*I, kon, koff, Df, R));
        ff = creal(cexp(wfti)*laplace_fun(sigma + wf*I, kon, koff, Df, R));
        sum += 0.5*(wf - wi)*(fi + ff);
        wi = wf;
    }

    return creal(sum*cexp(sigma*t)/M_PI);
}

int
model_f (const gsl_vector * x, void *data, 
        gsl_vector * f) {
    size_t n = ((struct data *)data)->n;
    double Df = ((struct data *)data)->Df;
    double R = ((struct data *)data)->R;
    double *time = ((struct data *)data)->time;
    double *y = ((struct data *)data)->y;
    double *sigma = ((struct data *) data)->sigma;
    char *m = ((struct data *) data)->m;

    double kon = gsl_vector_get (x, 0);
    double koff = gsl_vector_get (x, 1);

    size_t i;

    /* Importing one of the model functions */    
    double (*inverted_fun)();
    inverted_fun = &invlap;
    //char m[] = "fullModel";

    for (i = 0; i < n; i++) {
        /* Model Yi = A * exp(-lambda * i) + b */
        //double Yi = A * exp (-lambda * t) + b;

        double Yi = inverted_fun(time[i], kon, koff, Df, R, m, 0);
        gsl_vector_set (f, i, (Yi - y[i])/sigma[i]);
    }

    return GSL_SUCCESS;
}

int
model_df(const gsl_vector * x, void *data,
        gsl_matrix * J) {
    size_t n = ((struct data *)data)->n;
    double Df = ((struct data *)data)->Df;
    double R = ((struct data *)data)->R;
    double *time = ((struct data *) data)->time;
    double *sigma = ((struct data *) data)->sigma;
    char *m = ((struct data *) data)->m;

    double kon = gsl_vector_get (x, 0);
    double koff = gsl_vector_get (x, 1);

    size_t i;

    /* Importing one of the model functions */    
    double (*inverted_fun)();
    inverted_fun = &invlap;
    //char m[] = "fullModel";

    for (i = 0; i < n; i++) {
        /* Jacobian matrix J(i,j) = dfi / dxj, */
        /* where fi = (Yi - yi)/sigma[i],      */
        /*       Yi = A * exp(-lambda * i) + b  */
        /* and the xj are the parameters (A,lambda,b) */

        gsl_matrix_set (J, i, 0, inverted_fun(time[i], kon, koff, Df, R, m, 1)/sigma[i]);
        gsl_matrix_set (J, i, 1, inverted_fun(time[i], kon, koff, Df, R, m, 2)/sigma[i]);
    }

    return GSL_SUCCESS;
}

int
model_fdf(const gsl_vector * x, void *data,
         gsl_vector * f, gsl_matrix * J) {
    model_f (x, data, f);
    model_df (x, data, J);

    return GSL_SUCCESS;
}

void
print_state (size_t iter, gsl_multifit_fdfsolver * s) {
    printf ("iter: %3u x = % 15.8f % 15.8f "
            "|f(x)| = %g\n",
            iter,
            gsl_vector_get (s->x, 0),
            gsl_vector_get (s->x, 1),
            gsl_blas_dnrm2 (s->f));
}

void
bad_input(void) {
    fprintf(stderr, "Usage: cFDAP [-d diffusion_constant] [-r2 half_activation_area]\n");
    fprintf(stderr, "             [-m model_type] [-tini initial_time]\n");
    fprintf(stderr, "             [-tend end_time] [-n numsteps]\n");
    fprintf(stderr, "             [-kon0 initial_kon] [-koff0 initial_koff]\n");
    fprintf(stderr, "             [-i input] [-sd standard_deviation]\n");
    fprintf(stderr, "             [-o output]\n\n");
    fprintf(stderr, "  diffusion_constant:     diffusion constant of unbound proteins (default: 11.0 µm2/s)\n");
    fprintf(stderr, "  half_activation_area:   half length of the activation area (default: 3.0 µm)\n");
    fprintf(stderr, "  model_type:             reaction-diffusion model to fit with:\n");
    fprintf(stderr, "                          fullModel (default), hybridModel,\n");
    fprintf(stderr, "                          reactionDominantPure, effectiveDiffusion\n");
    fprintf(stderr, "  initial_time:           initial time in the curve duration range (default: 0.0 s)\n");
    fprintf(stderr, "  end_time:               end time in the curve duration range (default: 112.0 s)\n");
    fprintf(stderr, "  numsteps:               number of steps in the FDAP curve (default: 113)\n");
    fprintf(stderr, "  kon0:                   starting value for kon (default: 0.5)\n");
    fprintf(stderr, "  koff0:                  starting value for koff (default: 0.5)\n");
    fprintf(stderr, "  input:                  name of input curve file (mandatory)\n");
    fprintf(stderr, "  standard_error:         name of input SD file (mandatory)\n");
    fprintf(stderr, "  output:                 prefix name of output file (Example: -o tau441wt\n");
    fprintf(stderr, "                          makes cFDAP output 'tau441wt_fit_parameters.dat'\n");
    fprintf(stderr, "                          and 'tau441wt_fit_curve.dat')\n");
    fprintf(stderr, "\n\n");
    exit(1);
}

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/

/* MAIN */
int
main(int argc, char *argv[]) {

    int i, status;
    unsigned int iter = 0;

    /* DEFAULTS */
    char m[] = "fullModel";
    char curve_name[80], std_name[80], output_prefix[80];
    curve_name[0] = 0; std_name[0] = 0; output_prefix[0] = 0;
    //const size_t n = DEFAULT_N;
    int n = DEFAULT_N;
    double Df = DEFAULT_DF, R = DEFAULT_R;
    double t_ini = DEFAULT_T_INI, t_end = DEFAULT_T_END;
    double x_init[2] = { DEFAULT_KON_INIT, DEFAULT_KOFF_INIT };

    fprintf(stderr, "\n");
    fprintf(stderr, "  --------------   cFDAP 0.1.0 (C) 2015\n");
    fprintf(stderr, "  |*    cFDAP  |   Authors: Maxim Igaev, Frederik Sündermann\n");
    fprintf(stderr, "  | *          |   cFDAP is a fitting program for FDAP data\n");
    fprintf(stderr, "  |  **        |   http://www.neurobiologie.uni-osnabrueck.de/\n");
    fprintf(stderr, "  |    ********|   https://github.com/moozzz\n");
    fprintf(stderr, "  --------------   Email: maxim.igaev@biologie.uni-osnabrueck.de\n");
    fprintf(stderr, "\n");

    if ((argc < 2) || (argc > 22)) {
        bad_input();
    }

    /* First, a model must be chosen */
    if(strcmp(argv[1], "-m") != 0) {
        fprintf(stderr, "ERROR: Firstly, a model must be chosen.\n\n");
        exit(1);
    }
    else {
        if(argc == 2) {
            fprintf(stderr, "ERROR: Specify the model's name.\n\n");
            exit(1);
        }
        else {
            if(strcmp(argv[2], "fullModel") == 0 ||
               strcmp(argv[2], "hybridModel") == 0 ||
               strcmp(argv[2], "reactionDominantPure") == 0) {
                strcpy(m, argv[2]);
            }
            else if(strcmp(argv[2], "effectiveDiffusion") == 0) {
                //strcpy(m, argv[2]);
                fprintf(stderr, "ERROR: '%s' model is not supported so far.\n\n", argv[2]);
                exit(1);
            }
            else {
                fprintf(stderr, "ERROR: Unknown model '%s'\n\n", argv[2]);
                exit(1);
            }
        }
    }

    /* Optional parameters */
    for(i = 2; i < argc; i++) {
        if(strcmp(argv[i], "-d") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing diffusion coefficient.\n\n");
                exit(1);
            }
            Df = atof(argv[i + 1]);
            i++;
            if(Df <= 0.0) {
                fprintf(stderr, "ERROR: Would a zero or negative diffusion constant make sense?\n\n");  
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-r2") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing half area size.\n\n");
                exit(1);
            }
            R = atof(argv[i + 1]);
            i++;
            if(R <= 0.0) {
                fprintf(stderr, "ERROR: Would a zero or negative half area size make sense?\n\n");
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-kon0") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing initial value for kon0.\n\n");
                exit(1);
            }
            x_init[0] = atof(argv[i + 1]);
            i++;
            if(x_init[0] < 0.0) {
                fprintf(stderr, "ERROR: Would a negative kon make sense?\n\n");
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-koff0") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing initial value for koff0.\n\n");
                exit(1);
            }
            x_init[1] = atof(argv[i + 1]);
            i++;
            if(x_init[1] < 0.0) {
                fprintf(stderr, "ERROR: Would a negative koff make sense?\n\n");
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-tini") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing initial time.\n\n");
                exit(1);
            }
            t_ini = atof(argv[i + 1]);
            i++;
            if(t_ini < 0.0) {
                fprintf(stderr, "ERROR: Would a negative initial time make sense?\n\n");
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-tend") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing end time.\n\n");
                exit(1);
            }
            t_end = atof(argv[i + 1]);
            i++;
            if(t_end < t_ini) {
                fprintf(stderr, "ERROR: Would t_end < t_ini make sense?\n\n");
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-n") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: Missing number of time points.\n\n");
                exit(1);
            }
            n = atoi(argv[i + 1]);
            i++;
            if(n < 3) {
                fprintf(stderr, "ERROR: Your curve contatins less than 3 points? Are you kidding?\n\n");
                exit(1);
            }
        }
        else if(strcmp(argv[i], "-i") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: No input curve files given.\n\n");
                exit(1);
            }
            strcpy(curve_name, argv[i + 1]);
            i++;
        }
        else if(strcmp(argv[i], "-sd") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: No input SD files given.\n\n");
                exit(1);
            }
            strcpy(std_name, argv[i + 1]);
            i++;
        }
        else if(strcmp(argv[i], "-o") == 0) {
            if(i == argc - 1) {
                fprintf(stderr, "ERROR: No output prefix name given.\n\n");
                exit(1);
            }
            strcpy(output_prefix, argv[i + 1]);
            i++;
        }
        else {
            if(strncmp(argv[i], "-", 1) == 0) {
                fprintf(stderr, "\nERROR: Illegal option %s\n\n", argv[i]);
                bad_input();
            }
        }
    }

    /* Checking whether input and output file names were given */
    if (curve_name[0] == 0 || std_name[0] == 0 || output_prefix[0] == 0) {
        fprintf(stderr, "ERROR: File input/output is not defined correctly\n\n");
        exit(1);
    }

    const size_t p = 2;
    double time[n], y[n], sigma[n], best_fit[n];
    double stepSize = (t_end - t_ini)/(double) (n - 1);
    const gsl_multifit_fdfsolver_type *T;
    gsl_multifit_fdfsolver *s;
    gsl_matrix *covar = gsl_matrix_alloc (p, p); /* Covariance matrix */

    struct data d = { n, Df, R, time, y, sigma, m };

    gsl_multifit_function_fdf f;
    gsl_vector_view x = gsl_vector_view_array (x_init, p);

    f.f = &model_f;
    f.df = &model_df;
    f.fdf = &model_fdf;
    f.n = n;
    f.p = p;
    f.params = &d;

    /* Importing the data to be fitted */
    double temp1, temp2;
    FILE *input_curve = fopen(curve_name, "r");
    FILE *error_curve = fopen(std_name, "r");
    for(i = 0; i < NELEMS_1D(y); i++) {
        time[i] = t_ini + (double) i*stepSize;
        fscanf(input_curve, "%lf", &temp1);
        fscanf(error_curve, "%lf", &temp2);
        y[i] = temp1;
        sigma[i] = temp2;
        /*printf("data: %f %g %g\n", time[i], y[i], sigma[i]);*/
    }
    if(time[0] == 0.0) time[0] = 0.01;
    fclose(input_curve);
    fclose(error_curve);

    /* Initializing the solver */
    T = gsl_multifit_fdfsolver_lmsder;
    s = gsl_multifit_fdfsolver_alloc (T, n, p);
    gsl_multifit_fdfsolver_set (s, &f, &x.vector);

    print_state (iter, s);

    do {
        iter++;
        status = gsl_multifit_fdfsolver_iterate (s);

        printf ("current status = %s\n", gsl_strerror (status));

        print_state (iter, s);

        if (status)
            break;

        status = gsl_multifit_test_delta (s->dx, s->x, 1e-4, 1e-4);
    }
    while (status == GSL_CONTINUE && iter < 500);

    gsl_multifit_covar (s->J, 0.0, covar);

#define FIT(i) gsl_vector_get(s->x, i)
#define ERR(i) sqrt(gsl_matrix_get(covar,i,i))

    {
        double chi = gsl_blas_dnrm2(s->f);
        double dof = n - p;
        double c = GSL_MAX_DBL(1, chi / sqrt(dof));

        printf("\nchisq/dof = %g\n",  pow(chi, 2.0) / dof);
        printf ("kon        = %.5f +/- %.5f\n", FIT(0), c*ERR(0));
        printf ("koff       = %.5f +/- %.5f\n", FIT(1), c*ERR(1));
        printf ("bound      = %.5f +/- %.5f\n", 100.0 - 100.0/(1.0 + FIT(0)/FIT(1)), 100.0*(c*ERR(0)/FIT(1) - FIT(0)*c*ERR(1)/FIT(1)/FIT(1))/(1.0 + FIT(0)/FIT(1))/(1.0 + FIT(0)/FIT(1)));
    }

    printf ("\nSTATUS = %s\n\n", gsl_strerror (status));

    /* Writing the best fit */
    FILE *fit_curve = fopen(strcat(output_prefix, "_best_fit.dat"), "w");
    for(i = 0; i < NELEMS_1D(best_fit); i++) {
        //double t = i;
        fprintf(fit_curve, "%f\n", invlap(time[i], FIT(0), FIT(1), Df, R, m, 0));
    }
    fclose(fit_curve);

    gsl_multifit_fdfsolver_free (s);
    gsl_matrix_free (covar);

    return 0;
}