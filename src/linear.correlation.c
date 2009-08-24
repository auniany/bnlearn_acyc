#include "common.h"

static SEXP cov2(SEXP data, SEXP length);

/* Linear Correlation. */
SEXP fast_cor(SEXP x, SEXP y, SEXP length) {

  int i = 0, *n = INTEGER(length);
  double *sum, *xx = REAL(x), *yy = REAL(y);
  double xm = 0, ym = 0, xsd = 0, ysd = 0;
  SEXP res;

  PROTECT(res = allocVector(REALSXP, 1));
  sum = REAL(res);
  *sum = 0;

  /* compute the mean values  */
  for (i = 0 ; i < *n; i++) {

    xm += xx[i];
    ym += yy[i];

  }/*FOR*/

  xm /= (*n);
  ym /= (*n);

  /* compute the actual covariance. */
  for (i = 0; i < *n; i++) {

    *sum += (xx[i] - xm) * (yy[i] - ym);
    xsd += (xx[i] - xm) * (xx[i] - xm);
    ysd += (yy[i] - ym) * (yy[i] - ym);

  }/*FOR*/

  /* safety check against "divide by zero" errors. */
  if (xsd == 0 || ysd == 0)
    *sum = 0;
  else
    *sum /= sqrt(xsd) * sqrt(ysd);

  UNPROTECT(1);

  return res;

}/*FAST_COR*/

/* Partial Linear Correlation. */
SEXP fast_pcor(SEXP data, SEXP length) {

  int i = 0, ncols = LENGTH(data);
  SEXP res, cov, svd;
  double *u, *d, *vt;
  double k11 = 0, k12 = 0, k22 = 0;
  double tol = MACHINE_TOL;

  /* compute the single-value decomposition of the matrix. */
  PROTECT(cov = cov2(data, length));
  PROTECT(svd = r_svd(cov));

  /* extrack the three matrices form the list. */
  u = REAL(getListElement(svd, "u"));
  d = REAL(getListElement(svd, "d"));
  vt = REAL(getListElement(svd, "vt"));

  /* compute the three elements of the pseudoinverse I need
   * for the partial correlation coefficient. */
  for (i = 0; i < ncols; i++) {

    if (d[i] > tol) {

      k11 += u[CMC(0, i, ncols)] * vt[CMC(i, 0, ncols)] / d[i];
      k12 += u[CMC(0, i, ncols)] * vt[CMC(i, 1, ncols)] / d[i];
      k22 += u[CMC(1, i, ncols)] * vt[CMC(i, 1, ncols)] / d[i];

    }/*THEN*/

  }/*FOR*/

  /* allocate and initialize the return value. */
  PROTECT(res = allocVector(REALSXP, 1));
  NUM(res) = -k12 / sqrt(k11 * k22);

  UNPROTECT(3);
  return res;

}/*FAST_PCOR*/

static SEXP cov2(SEXP data, SEXP length) {

  int i = 0, j = 0, k = 0, cur = 0;
  int *n = INTEGER(length), ncols = LENGTH(data);
  double *mean, *var, **column;
  SEXP res;

  /* allocate the covariance matrix. */
  PROTECT(res = allocMatrix(REALSXP, ncols, ncols));
  var = REAL(res);
  memset(var, '\0', ncols * ncols * sizeof(double));

  /* allocate an array to store the mean values. */
  mean = Calloc(ncols, double);
  memset(mean, '\0', sizeof(double) * ncols);

  /* allocate and initialize an array of pointers for the variables. */
  column = (double **) Calloc(ncols, double *);
  for (i = 0; i < ncols; i++)
    column[i] = REAL(VECTOR_ELT(data, i));

  /* compute the mean values  */
  for (i = 0; i < ncols; i++) {

    for (j = 0 ; j < *n; j++)
      mean[i] += column[i][j];

    mean[i] /= (*n);

  }/*FOR*/

  /* compute the actual covariance. */
  for (i = 0; i < ncols; i++) {

    for (j = i; j < ncols; j++) {

      cur = CMC(i, j, ncols);

      for (k = 0; k < *n; k++)
        var[cur] += (column[i][k] - mean[i]) * (column[j][k] - mean[j]);

      var[cur] /= (*n) - 1;

      /* fill in the symmetric element of the matrix. */
      var[CMC(j, i, ncols)] = var[cur];

    }/*FOR*/

  }/*FOR*/

  Free(column);
  Free(mean);
  UNPROTECT(1);
  return res;

}/*COV2*/
