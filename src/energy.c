/*
   energy.c: energy package

   Author:  Maria Rizzo <mrizzo at bgsu.edu>
   Created: 4 Jan 2004
   Updated: 2 April 2008    some functions moved to utilities.c
   Updated: 25 August 2016  mvnEstat converted to c++ in mvnorm.cpp
   Updated: 16 February 2021  poisMstat ported to Rcpp in poissonM.cpp

   ksampleEtest() performs the multivariate E-test for equal distributions,
                  complete version, from data matrix
   E2sample()     computes the 2-sample E-statistic without creating distance
*/

#include <R.h>
#include <Rmath.h>

void   ksampleEtest(double *x, int *byrow, int *nsamples, int *sizes, int *dim,
            int *R, double *e0, double *e, double *pval, int *U);
void   E2sample(double *x, int *sizes, int *dim, double *stat);

double edist(double **D, int m, int n, int unbiased);
double multisampleE(double **D, int nsamples, int *sizes, int *perm, int unbiased);
double twosampleE(double **D, int m, int n, int *xrows, int *yrows, int unbiased);
double E2(double **x, int *sizes, int *start, int ncol, int *perm);
double Eksample(double *x, int *byrow, int r, int d, int K, int *sizes, int *ix);
void   distance(double **bxy, double **D, int N, int d);

/* utilities.c */
extern double **alloc_matrix(int r, int c);
extern int    **alloc_int_matrix(int r, int c);
extern void   free_matrix(double **matrix, int r, int c);
extern void   free_int_matrix(int **matrix, int r, int c);

extern void   permute(int *J, int n);
extern void   roworder(double *x, int *byrow, int r, int c);
extern void   vector2matrix(double *x, double **y, int N, int d, int isroworder);

extern void   distance(double **bxy, double **D, int N, int d);
extern void   Euclidean_distance(double *x, double **Dx, int n, int d);
extern void   index_distance(double *x, double **Dx, int n, int d, double index);
extern void   sumdist(double *x, int *byrow, int *nrow, int *ncol, double *lowersum);


void E2sample(double *x, int *sizes, int *dim, double *stat) {
    /*
      compute test statistic *stat for testing H:F=G
      does not store distance matrix
      x must be in row order: x=as.double(t(x)) where
      x is pooled sample in matrix sum(en) by dim
    */
    int    m=sizes[0], n=sizes[1], d=(*dim);
    int    i, j, k, p, q;
    double dif, dsum, sumxx, sumxy, sumyy, w;

    sumxy = 0.0;
    for (i=0; i<m; i++) {
        p = i*d;
        for (j=m; j<m+n; j++) {
            dsum = 0.0;
            q = j*d;
            for (k=0; k<d; k++) {
                dif = *(x+p+k) - *(x+q+k);
                dsum += dif*dif;
            }
            sumxy += sqrt(dsum);
        }
    }
    sumxy /= (double)(m*n);
    sumxx = 0.0;
    for (i=1; i<m; i++) {
        p = i*d;
        for (j=0; j<i; j++) {
            dsum = 0.0;
            q = j*d;
            for (k=0; k<d; k++) {
                dif = *(x+p+k) - *(x+q+k);
                dsum += dif*dif;
            }
            sumxx += sqrt(dsum);
        }
    }
    sumxx /= (double)(m*m);  /* half the sum */
    sumyy = 0.0;
    for (i=m+1; i<m+n; i++) {
        p = i*d;
        for (j=m; j<i; j++) {
            dsum = 0.0;
            q = j*d;
            for (k=0; k<d; k++) {
                dif = *(x+p+k) - *(x+q+k);
                dsum += dif*dif;
            }
            sumyy += sqrt(dsum);
        }
    }
    sumyy /= (double)(n*n);  /* half the sum */
    w = (double)(m*n)/(double)(m+n);
    *stat = 2.0*w*(sumxy - sumxx - sumyy);
}

void ksampleEtest(double *x, int *byrow,
                  int *nsamples, int *sizes, int *dim,
                  int *R, double *e0, double *e, double *pval,
                  int *U)
{
    /*
      exported for R energy package: E test for equal distributions
      x         the pooled sample (or distances)
      *byrow    logical, TRUE if x is stored by row
                pass x=as.double(t(x)) if *byrow==TRUE
      *nsamples number of samples
      *sizes    vector of sample sizes
      *dim      dimension of data in x (0 if x is distance matrix)
      *R        number of replicates for permutation test
      *e0       observed E test statistic
      e         vector of replicates of E statistic
      *pval     approximate p-value
    */

    int    b, ek, i, k;
    int    B = (*R), K = (*nsamples), d=(*dim), N;
    int    *perm;
    double **data, **D;

    N = 0;
    for (k=0; k<K; k++)
        N += sizes[k];
    perm = Calloc(N, int);
    for (i=0; i<N; i++)
        perm[i] = i;
    D   = alloc_matrix(N, N);      /* distance matrix */
    if (d > 0) {
        data = alloc_matrix(N, d); /* sample matrix */
        vector2matrix(x, data, N, d, *byrow);
        distance(data, D, N, d);
        free_matrix(data, N, d);
    }
    else
        vector2matrix(x, D, N, N, *byrow);

    *e0 = multisampleE(D, K, sizes, perm, *U);

    /* bootstrap */
    if (B > 0) {
        ek = 0;
        GetRNGstate();
        for (b=0; b<B; b++) {
            permute(perm, N);
            e[b] = multisampleE(D, K, sizes, perm, *U);
            if ((*e0) < e[b]) ek++;
        }
        PutRNGstate();
        (*pval) = ((double) (ek + 1)) / ((double) (B + 1));
    }

    free_matrix(D, N, N);
    Free(perm);
}



double E2(double **x, int *sizes, int *start, int ncol, int *perm)
{
    int    m=sizes[0], n=sizes[1];
    int    row1=start[0], row2=start[1];
    int    i, j, k, p, q;
    double dif, dsum, sumxx, sumxy, sumyy, w;

    sumxy = 0.0;
    for (i=0; i<m; i++) {
        p = perm[row1 + i];
        for (j=0; j<n; j++) {
            dsum = 0.0;
            q = perm[row2 + j];
            for (k=0; k<ncol; k++) {
                dif = x[p][k] - x[q][k];
                dsum += dif * dif;
            }
            sumxy += sqrt(dsum);
        }
    }
    sumxy /= (double)(m * n);
    sumxx = 0.0;
    for (i=1; i<m; i++) {
        p = perm[row1 + i];
        for (j=0; j<i; j++) {
            dsum = 0.0;
            q = perm[row1 + j];
            for (k=0; k<ncol; k++) {
                dif = x[p][k] - x[q][k];
                dsum += dif * dif;
            }
            sumxx += sqrt(dsum);
        }
    }
    sumxx /= (double)(m * m);  /* half the sum */
    sumyy = 0.0;
    for (i=1; i<n; i++) {
        p = perm[row2 + i];
        for (j=0; j<i; j++) {
            dsum = 0.0;
            q = perm[row2 + j];
            for (k=0; k<ncol; k++) {
                dif = x[p][k] - x[q][k];
                dsum += dif * dif;
            }
            sumyy += sqrt(dsum);
        }
    }
    sumyy /= (double)(n * n);  /* half the sum */
    w = (double)(m * n)/(double)(m + n);
    return 2.0 * w * (sumxy - sumxx - sumyy);
}


double multisampleE(double **D, int nsamples, int *sizes, int *perm, int unbiased)
{
    /*
      returns the multisample E statistic
      D is square Euclidean distance matrix
      perm is a permutation of the row indices
    */
    int i, j, k, m, n;
    int *M;
    double e;

    M = Calloc(nsamples, int);
    M[0] = 0;
    for (k=1; k<nsamples; k++)
        M[k] = M[k-1] + sizes[k-1]; /* index where sample k begins */

    e = 0.0;
    for (i=0; i<nsamples; i++) {
        m = sizes[i];
        for (j=i+1; j<nsamples; j++) {
            n = sizes[j];
            e += twosampleE(D, m, n, perm+M[i], perm+M[j], unbiased);
        }
    }
    Free(M);
    return(e);
}

double twosampleE(double **D, int m, int n, int *xrows, int *yrows, int unbiased)
{
    /*
       return the e-distance between two samples
       corresponding to samples indexed xrows[] and yrows[]
       D is square Euclidean distance matrix
    */
    int    i, j;
    double sumxx=0.0, sumyy=0.0, sumxy=0.0;

    if (m < 1 || n < 1) return 0.0;
    for (i=0; i<m; i++)
        for (j=i+1; j<m; j++)
            sumxx += D[xrows[i]][xrows[j]];
    sumxx *= 2.0/((double)(m*m));
    for (i=0; i<n; i++)
        for (j=i+1; j<n; j++)
            sumyy += D[yrows[i]][yrows[j]];
    sumyy *= 2.0/((double)(n*n));
    if (unbiased == 1) {
      sumxx *= (double)(m) / (double)(m - 1);
      sumyy *= (double)(n) / (double)(n - 1);
    }

    for (i=0; i<m; i++)
        for (j=0; j<n; j++)
            sumxy += D[xrows[i]][yrows[j]];
    sumxy /= ((double) (m*n));

    return (double)(m*n)/((double)(m+n)) * (2*sumxy - sumxx - sumyy);
}

double edist(double **D, int m, int n, int unbiased)
{
    /*
      return the e-distance between two samples size m and n
      D is square Euclidean distance matrix
    */
    int    i, j;
    double sumxx=0.0, sumyy=0.0, sumxy=0.0;

    if (m < 1 || n < 1) return 0.0;
    for (i=0; i<m; i++)
        for (j=i+1; j<m; j++)
            sumxx += D[i][j];
    sumxx *= 2.0/((double)(m*m));
    for (i=0; i<n; i++)
        for (j=i+1; j<n; j++)
            sumyy += D[i][j];
    sumyy *= 2.0/((double)(n*n));
    if (unbiased == 1) {
      sumxx *= (double)(m) / (double)(m - 1);
      sumyy *= (double)(n) / (double)(n - 1);
    }

    for (i=0; i<m; i++)
        for (j=0; j<n; j++)
            sumxy += D[i][j];
    sumxy /= ((double) (m*n));
    return (double)(m*n)/((double)(m+n)) * (2*sumxy - sumxx - sumyy);
}

