#include "common.h"

SEXP has_pdag_path(SEXP from, SEXP to, SEXP amat, SEXP nrows, SEXP nodes,
    SEXP underlying, SEXP exclude_direct, SEXP debug) {

int start = INT(from) - 1, stop = INT(to) - 1, n = INT(nrows);
int debuglevel = LOGICAL(debug)[0], notdirect = LOGICAL(exclude_direct)[0],
      ugraph = LOGICAL(underlying)[0], *a = INTEGER(amat);
SEXP result;

  PROTECT(result = allocVector(LGLSXP, 1));
  LOGICAL(result)[0] =
    c_has_path(start, stop, a, n, nodes, ugraph, notdirect, debuglevel);

  UNPROTECT(1);
  return result;

}/*HAS_DAG_PATH*/

int c_has_path(int start, int stop, int *amat, int n, SEXP nodes,
    int ugraph, int notdirect, int debuglevel) {

int *counter = NULL, *path = NULL;
int i = 0, a1 = 0, a2 = 0, path_pos = 0, cur = start;

  /* remove any arc between start and stop if asked to. */
  if (notdirect) {

    a1 = amat[CMC(start, stop, n)];
    a2 = amat[CMC(stop, start, n)];
    amat[CMC(start, stop, n)] =  amat[CMC(stop, start, n)] = 0;

  }/*THEN*/

  /* initialize the position counters for the rows of the adjacency matrix. */
  counter = alloc1dcont(n);
  /* initialize the path array. */
  path = alloc1dcont(n);

  /* iterate until the other node is found. */
  while (cur != stop) {

    if (debuglevel > 0) {

      Rprintf("* currently at '%s'.\n", NODE(cur));
      Rprintf("  > current path is:\n");
      for (int i = 0; i < path_pos ; i ++)
        Rprintf("'%s' ", NODE(path[i]));
      Rprintf("'%s' \n", NODE(cur));

    }/*THEN*/

there:

    /* find the next child of the 'cur' node. */
    for (i = 0; (i < n) && (counter[cur] < n); i++) {

      if (!ugraph) {

        if (amat[CMC(cur, counter[cur], n)] != 0)
          break;

      }/*THEN*/
      else {

        /* if we are looking for a path in the underlying (undirected)
         * graph, check also the symmetric entry of the adjacency matrix. */
        if ((amat[CMC(cur, counter[cur], n)] != 0) ||
            (amat[CMC(counter[cur], cur, n)] != 0))
          break;

      }/*ELSE*/

      counter[cur]++;

    }/*FOR*/

    /* the column indexes range from 0 to n - 1;  counter value of n means
     * that all the children of that node has beeen visited. */
    if (counter[cur] == n) {

      /* if this node is the first one in the path, there search is finished
       * and the 'stop' node was not found; return FALSE. */
      if  (path_pos == 0) {

        /* remove any arc between start and stop if asked to. */
        if (notdirect) {

          amat[CMC(start, stop, n)] = a1;
          amat[CMC(stop, start, n)] = a2;

        }/*THEN*/

        return FALSE;

      }/*THEN*/

      if (debuglevel > 0)
        Rprintf("  > node '%s' has no more children, going back to '%s'.\n",
          NODE(cur), NODE(path[path_pos - 1]));

      /* this node has no more children, skip back to the previous one. */
      cur = path[--path_pos];
      path[path_pos + 1] = 0;

      goto there;

    }/*THEN*/
    else {

      /* inrement the counter to get to the next node to check, unless
       * that one was the last. */
      if (counter[cur] < n)
        counter[cur]++;

      /* do not visit an already visited node */
      for (i = path_pos - 1; i >= 0; i--) {

        if ((counter[cur] - 1) == path[i]) {

          if (debuglevel > 0)
            Rprintf("  @ node '%s' already visited, skipping.\n", NODE(path[i]));

          goto there;

        }/*THEN*/

      }/*FOR*/

      /* update the path. */
      path[path_pos++] = cur;
      /* the current node is now the children we have just found. */
      cur = counter[cur] - 1;

      if (debuglevel > 0)
        Rprintf("  > jumping to '%s'.\n", NODE(cur));

    }/*ELSE*/

  }/*WHILE*/


  /* remove any arc between start and stop if asked to. */
  if (notdirect) {

    amat[CMC(start, stop, n)] = a1;
    amat[CMC(stop, start, n)] = a2;

  }/*THEN*/

  /* node 'stop' has been found, return TRUE. */
  return TRUE;

}/*C_HAS_DAG_PATH*/

SEXP how_many_cycles(SEXP from, SEXP to, SEXP amat, SEXP nrows, SEXP nodes, SEXP debug) {

int start = INT(from) - 1, stop = INT(to) - 1, n = INT(nrows);
int *counter = NULL, *path = NULL, *a = NULL;
int path_pos = 0, cur = start, cycle_counter = 0;
int i = 0, a1 = 0, a2 = 0;
int *debuglevel = NULL;
SEXP res;

  /* dereference the debug parameter. */
  debuglevel = LOGICAL(debug);

  /* initialize the position counters for the rows of the adjacency matrix. */
  counter = alloc1dcont(n);
  /* initialize the path array. */
  path = alloc1dcont(n);

  /* allocate the result. */
  PROTECT(res = allocVector(INTSXP, 1));
  INT(res) = 0;

  /* map the adjacency matrix. */
  a = INTEGER(amat);
  /* back up and remove any arc between "start" and "stop".  */
  a1 = a[CMC(start, stop, n)];
  a2 = a[CMC(stop, start, n)];
  a[CMC(start, stop, n)] = a[CMC(stop, start, n)] = 0;

  while (1) {

    if (*debuglevel > 0)
      Rprintf("* currently at '%s'.\n", NODE(cur));

there:

    /* find the next child of the 'cur' node. */
    for (i = 0; (i < n) && (counter[cur] < n); i++) {

      if (a[CMC(cur, counter[cur], n)] != 0)
        break;

      counter[cur]++;

    }/*FOR*/

    if (cur == stop) {

      cycle_counter++;

      if (*debuglevel > 0) {

        Rprintf("  @ found node '%s' ! cycle counter is now %d.\n",
          NODE(stop), cycle_counter);
        Rprintf("  @ the cycle is:\n'%s' ", NODE(stop));
        for (int i = 0; i < path_pos ; i ++)
          Rprintf("'%s' ", NODE(path[i]));
        Rprintf("'%s' \n", NODE(cur));

      }/*THEN*/

      /* do not go on on this path. */
      if (counter[cur] < n)
        counter[cur]++;
      cur = path[--path_pos];
      path[path_pos + 1] = 0;

    }/*THEN*/
    else if (counter[cur] == n) {

      /* the column indexes range from 0 to n - 1;  counter value of n means
       * that all the children of that node has beeen visited.  */

      /* if this node is the first one in the path, there search is finished
       * and the 'stop' node was not found; return FALSE.*/
      if  (path_pos == 0) {

        INT(res) = cycle_counter;
        UNPROTECT(1);

        /* restore the original values in the adjacency matrix. */
        a[CMC(start, stop, n)] = a1;
        a[CMC(stop, start, n)] = a2;

        return res;

      }/*THEN*/

      if (*debuglevel > 0) {

        Rprintf("  > node '%s' has no more children, going back to '%s'.\n",
          NODE(cur), NODE(path[path_pos - 1]));

      }/*THEN*/

      /* this node has no more children, skip back to the previous one. */
      cur = path[--path_pos];
      path[path_pos + 1] = 0;

    }/*THEN*/
    else {

      /* inrement the counter to get to the next node to check, unless
       * that one was the last. */
      if (counter[cur] < n)
        counter[cur]++;

      /* do not visit an already visited node */
      for (i = path_pos - 1; i >= 0; i--) {

        if ((counter[cur] - 1) == path[i]) {

          if (*debuglevel > 0) {

            Rprintf("  @ node '%s' already visited, skipping.\n", NODE(path[i]));

          }/*THEN*/

          goto there;

        }/*THEN*/

      }/*FOR*/

      /* update the path. */
      path[path_pos++] = cur;
      /* the current node is now the children we have just found. */
      cur = counter[cur] - 1;

      if (*debuglevel > 0) {

        Rprintf("  > jumping to '%s'.\n", NODE(cur));

      }/*THEN*/

    }/*ELSE*/

  }/*WHILE*/

}/*HOW_MANY_CYCLES*/

