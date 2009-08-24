#include "common.h"

static void bestop_update(SEXP bestop, char *op, const char *from, const char *to);

SEXP hc_cache_fill(SEXP nodes, SEXP data, SEXP network, SEXP score, SEXP extra, 
    SEXP reference, SEXP equivalence, SEXP updated, SEXP env, SEXP amat, 
    SEXP cache, SEXP blmat, SEXP debug) {

  int *colsum = NULL, nnodes = LENGTH(nodes), lupd = LENGTH(updated);
  int *a, *upd, *b, i = 0, j = 0, k = 0;
  double *cache_value;
  SEXP arc, callenv, params, delta, op, dummy, temp;

  /* save a pointer to the adjacency matrix, the blacklist and the 
   * updated nodes. */
  a = INTEGER(amat);
  b = INTEGER(blmat);
  upd = INTEGER(updated);

  /* if there are no nodes to update, return. */
  if (lupd == 0) return cache;

  /* set up row and column total to check for score equivalence;
   * zero means no parent nodes. */
  if (isTRUE(equivalence)) {

    colsum = alloc1dcont(nnodes);

    for (i = 0; i < nnodes; i++)
      for (j = 0; j < nnodes; j++)
        colsum[j] += a[CMC(i, j, nnodes)];

  }/*THEN*/

  /* allocate and initialize the cache. */
  cache_value = REAL(cache);

  /* allocate a two-slot character vector. */
  PROTECT(arc = allocVector(STRSXP, 2));

  /* allocate the call environment fro the call to score.delta. */
  PROTECT(params = callenv = allocList(10));
  SET_TYPEOF(callenv, LANGSXP);

  /* allocate and initialize the fake score delta. */
  PROTECT(delta = allocVector(REALSXP, 1));
  NUM(delta) = 0;

  /* allocate and initialize the score.delta() operator. */
  PROTECT(op = allocVector(STRSXP, 1));
  SET_STRING_ELT(op, 0, mkChar("set"));

  /* allocate and initialize the debug parameter. */
  PROTECT(dummy = allocVector(LGLSXP, 1));
  LOGICAL(dummy)[0] = FALSE;

  /* set up the call to score.delta(). */
  SETCAR(params, install("score.delta"));
  params = CDR(params);
  SETCAR(params, arc);
  params = CDR(params);
  SETCAR(params, network);
  params = CDR(params);
  SETCAR(params, data);
  params = CDR(params);
  SETCAR(params, score);
  params = CDR(params);
  SETCAR(params, delta);
  params = CDR(params);
  SETCAR(params, reference);
  params = CDR(params);
  SETCAR(params, op);
  params = CDR(params);
  SETCAR(params, extra);
  params = CDR(params);
  SETCAR(params, dummy);

  for (i = 0; i < nnodes; i++) {

    for (j = 0; j < nnodes; j++) {

       /* incident nodes must be different from each other. */
       if (i == j) continue;

       /* if only one or two nodes' caches need updating, skip the rest. */
       for (k = 0; k < lupd; k++) 
         if (upd[k] == j) 
           goto there;

       continue;  

there:

       /* no need to compute the score delta for blacklisted arcs. */
       if (b[CMC(i, j, nnodes)] == 1)
         continue;

       /* use score equivalence if possible to check only one orientation. */
       if (isTRUE(equivalence)) {

         /* if the following conditions are met, look up the score delta of 
          * the reverse of the current arc: 
          *   1) that score delta has already been computed.
          *   2) both incident nodes have no parent, so the arc is really
          *      score equivalent (no v-structures). 
          *   3) the reversed arc has not been blacklisted, as the score delta
          *      is not computed in this case. */
         if ((i > j) && (colsum[i] + colsum[j] == 0) && (b[CMC(j, i, nnodes)] == 0)) {

           cache_value[CMC(i, j, nnodes)] = cache_value[CMC(j, i, nnodes)];
           continue;

         }/*THEN*/

       }/*THEN*/

       /* save the nodes incident on the arc. */
       SET_STRING_ELT(arc, 0, STRING_ELT(nodes, i));
       SET_STRING_ELT(arc, 1, STRING_ELT(nodes, j));

       /* add the arc to the call to score.delta().  */
       params = CDR(callenv);
       SETCAR(params, arc);

       /* if the arc is not present in the graph it should be added; 
        * otherwise it should be removed. */
       for (k = 0; k < 6; k++) params = CDR(params);

       if (a[CMC(i, j, nnodes)] == 0)
         SET_STRING_ELT(op, 0, mkChar("set"));
       else
         SET_STRING_ELT(op, 0, mkChar("drop"));
  
       SETCAR(params, op);

       /* evaluate the call to score.delta() for the arc. */
       PROTECT(temp = eval(callenv, env));
       cache_value[CMC(i, j, nnodes)] = NUM(VECTOR_ELT(temp, 1));
       UNPROTECT(1);

       if (isTRUE(debug))
         Rprintf("* caching score delta for arc %s -> %s (%lf).\n", 
           CHAR(STRING_ELT(nodes, i)), CHAR(STRING_ELT(nodes, j)),
            cache_value[CMC(i, j, nnodes)]);

    }/*FOR*/

  }/*FOR*/

  UNPROTECT(5);
  return cache;

}/*HC_CACHE_FILL*/

#define NODE(i) CHAR(STRING_ELT(nodes, i))

/* a single step of the optimized hill climbing (one arc addition/removal/reversal). */
SEXP hc_opt_step(SEXP amat, SEXP nodes, SEXP added, SEXP cache, SEXP reference,
    SEXP wlmat, SEXP blmat, SEXP debug) {

  int nnodes = LENGTH(nodes), i = 0, j = 0;
  int *am, *ad, *w, *b, counter = 0, update = 1, from = 0, to = 0;
  double *cache_value, temp, max = 0;
  SEXP false, true, bestop, names;

  /* allocate and initialize the debug parameter. */
  PROTECT(false = allocVector(LGLSXP, 1));
  LOGICAL(false)[0] = FALSE;
  PROTECT(true = allocVector(LGLSXP, 1));
  LOGICAL(true)[0] = TRUE;

  /* allocate and initialize the return value (use FALSE as a canary value). */
  PROTECT(bestop = allocVector(VECSXP, 3));
  PROTECT(names = allocVector(STRSXP, 3));
  SET_STRING_ELT(names, 0, mkChar("op"));
  SET_STRING_ELT(names, 1, mkChar("from"));
  SET_STRING_ELT(names, 2, mkChar("to"));
  setAttrib(bestop, R_NamesSymbol, names);
  SET_VECTOR_ELT(bestop, 0, false);

  /* save pointers to the numeric/integer matrices. */
  cache_value = REAL(cache);
  ad = INTEGER(added);
  am = INTEGER(amat);
  w = INTEGER(wlmat);
  b = INTEGER(blmat);

  if (isTRUE(debug)) {

     /* count how may arcs are to be tested. */
     for (i = 0; i < nnodes * nnodes; i++)
       counter += ad[i];

     Rprintf("----------------------------------------------------------------\n");
     Rprintf("* trying to add one of %d arcs.\n", counter);

  }/*THEN*/

  for (i = 0; i < nnodes; i++) {

    for (j = 0; j < nnodes; j++) {

      /* nothing to see, move along. */
      if (ad[CMC(i, j, nnodes)] == 0)
        continue;

      /* retrieve the score delta from the cache. */
      temp = cache_value[CMC(i, j, nnodes)];

      if (isTRUE(debug)) {

        Rprintf("  > trying to add %s -> %s.\n", NODE(i), NODE(j));
        Rprintf("    > delta between scores for nodes %s %s is %lf.\n", 
          NODE(i), NODE(j), temp);

      }/*THEN*/

      /* this score delta is the best one at the moment, so add the arc if it 
       * does not introduce cycles in the graph. */
      if (temp - max > MACHINE_TOL) {

        if (isTRUE(c_has_path(j, i, am, nnodes, nodes, false, false, false))) {

          if (isTRUE(debug))
            Rprintf("    > not adding, introduces cycles in the graph.\n");

          continue;

        }/*THEN*/

        if (isTRUE(debug)) 
          Rprintf("    @ adding %s -> %s.\n", NODE(i), NODE(j));

        /* update the return value. */
        bestop_update(bestop, "set", NODE(i), NODE(j));
        /* store the node indices to update the reference scores. */
        from = i;
        to = j;

        /* update the threshold score delta. */
        max = temp;

      }/*THEN*/

    }/*FOR*/

  }/*FOR*/

  if (isTRUE(debug)) {

     /* count how may arcs are to be tested. */
     for (i = 0, counter = 0; i < nnodes * nnodes; i++)
       counter += am[i] * (1 - w[i]);

     Rprintf("----------------------------------------------------------------\n");
     Rprintf("* trying to remove one of %d arcs.\n", counter);

  }/*THEN*/

  for (i = 0; i < nnodes; i++) {

    for (j = 0; j < nnodes; j++) {

      /* nothing to see, move along. */
      if (am[CMC(i, j, nnodes)] == 0)
        continue;

      /* whitelisted arcs are not to be removed, ever. */
      if (w[CMC(i, j, nnodes)] == 1)
        continue;

      /* retrieve the score delta from the cache. */
      temp = cache_value[CMC(i, j, nnodes)];

      if (isTRUE(debug)) {

        Rprintf("  > trying to remove %s -> %s.\n", NODE(i), NODE(j));
        Rprintf("    > delta between scores for nodes %s %s is %lf.\n", 
          NODE(i), NODE(j), temp);

      }/*THEN*/

      if (temp - max > MACHINE_TOL) {

        if (isTRUE(debug))
          Rprintf("    @ removing %s -> %s.\n", NODE(i), NODE(j));

        /* update the return value. */
        bestop_update(bestop, "drop", NODE(i), NODE(j));
        /* store the node indices to update the reference scores. */
        from = i;
        to = j;

        /* update the threshold score delta. */
        max = temp;

      }/*THEN*/

    }/*FOR*/

  }/*FOR*/

  if (isTRUE(debug)) {

     /* count how may arcs are to be tested. */
     for (i = 0, counter = 0; i < nnodes; i++)
       for (j = 0; j < nnodes; j++)
         counter += am[CMC(i, j, nnodes)] * (1 - b[CMC(j, i, nnodes)]);

     Rprintf("----------------------------------------------------------------\n");
     Rprintf("* trying to reverse one of %d arcs.\n", counter);

  }/*THEN*/

  for (i = 0; i < nnodes; i++) {

    for (j = 0; j < nnodes; j++) {

      /* nothing to see, move along. */
      if (am[CMC(i, j, nnodes)] == 0)
        continue;

      /* don't reverse an arc if the one in the opposite direction is
       * blacklisted, ever. */
      if (b[CMC(j, i, nnodes)] == 1)
        continue;

      /* retrieve the score delta from the cache. */
      temp = cache_value[CMC(i, j, nnodes)] + cache_value[CMC(j, i, nnodes)];

      if (isTRUE(debug)) {

        Rprintf("  > trying to reverse %s -> %s.\n", NODE(i), NODE(j));
        Rprintf("    > delta between scores for nodes %s %s is %lf.\n", 
          NODE(i), NODE(j), temp);

      }/*THEN*/

      if (temp - max > MACHINE_TOL) {

        if (isTRUE(c_has_path(i, j, am, nnodes, nodes, false, true, false))) {

          if (isTRUE(debug))
            Rprintf("    > not reversing, introduces cycles in the graph.\n");

          continue;

        }/*THEN*/

        if (isTRUE(debug))
          Rprintf("    @ reversing %s -> %s.\n", NODE(i), NODE(j));

        /* update the return value. */
        bestop_update(bestop, "reverse", NODE(i), NODE(j));
        /* store the node indices to update the reference scores. */
        from = i;
        to = j;
        /* both nodes' reference scores must be updated. */
        update = 2;

        /* update the threshold score delta. */
        max = temp;

      }/*THEN*/

    }/*FOR*/

  }/*FOR*/

  /* update the reference scores. */
  REAL(reference)[to] += cache_value[CMC(from, to, nnodes)];
  if (update == 2)
    REAL(reference)[from] += cache_value[CMC(to, from, nnodes)];

  UNPROTECT(4);

  return bestop;

}/*HC_OPT_STEP*/

static void bestop_update(SEXP bestop, char *op, const char *from, const char *to) {

  SEXP temp;

  PROTECT(temp = allocVector(STRSXP, 1));

  SET_STRING_ELT(temp, 0, mkChar(op));
  SET_VECTOR_ELT(bestop, 0, duplicate(temp));

  SET_STRING_ELT(temp, 0, mkChar(from));
  SET_VECTOR_ELT(bestop, 1, duplicate(temp));

  SET_STRING_ELT(temp, 0, mkChar(to));
  SET_VECTOR_ELT(bestop, 2, duplicate(temp));

  UNPROTECT(1);

}/*BESTOP_UPDATE*/

SEXP hc_to_be_added(SEXP arcs, SEXP blacklist, SEXP nodes, SEXP convert) {

  SEXP amat, blmat, result, dimnames;
  int i = 0, j = 0, dims = LENGTH(nodes);
  int *res, *a, *b, coords = 0;

  /* transform the arc set into an adjacency matrix, if it's not one already. */
  if (isInteger(arcs)) {

    amat = arcs;

  }/*THEN*/
  else {

    PROTECT(amat = arcs2amat(arcs, nodes));

  }/*ELSE*/

  /* do the same for the blacklist. */
  if (isInteger(blacklist) || isNull(blacklist)) {

    blmat = blacklist;

  }/*THEN*/
  else {

    PROTECT(blmat = arcs2amat(blacklist, nodes));

  }/*ELSE*/

  a = INTEGER(amat);

  /* allocate and initialize the return value. */
  PROTECT(result = allocMatrix(INTSXP, dims, dims));
  res = INTEGER(result);
  memset(res, '\0', sizeof(int) * dims * dims);

  if (isNull(blmat)) {

    for (i = 0; i < dims; i++) {

      for (j = 0; j < dims; j++) {

        /* diagonal elements are always zero, nothing to do. */
        if (i == j) continue;
        /* save the matrix element's coordinates (to compute them only once). */
        coords = CMC(i, j, dims);

        res[coords] = 1 * (1 - a[coords]) * (1 - a[CMC(j, i, dims)]);

      }/*FOR*/

    }/*FOR*/

  }/*THEN*/
  else {

    b = INTEGER(blmat);

    for (i = 0; i < dims; i++) {

      for (j = 0; j < dims; j++) {

        /* diagonal elements are always zero, nothing to do. */
        if (i == j) continue;
        /* save the matrix element's coordinates (to compute them only once). */
        coords = CMC(i, j, dims);

        res[coords] = 1 * (1 - a[coords]) * (1 - a[CMC(j, i, dims)]) * (1 - b[coords]);

      }/*FOR*/

    }/*FOR*/

  }/*ELSE*/

  if (isTRUE(convert)) {

    PROTECT(result = amat2arcs(result, nodes));

  }/*THEN*/
  else {

    /* allocate rownames and colnames. */
    PROTECT(dimnames = allocVector(VECSXP, 2));
    SET_VECTOR_ELT(dimnames, 0, nodes);
    SET_VECTOR_ELT(dimnames, 1, nodes);
    setAttrib(result, R_DimNamesSymbol, dimnames);    

  }/*ELSE*/

  UNPROTECT(2);

  if (!isInteger(arcs))
    UNPROTECT(1);
  if (!(isInteger(blacklist) || isNull(blacklist)))
    UNPROTECT(1);

  return result;

}/*HC_TO_BE_ADDED*/
