#ifndef DEPTEST_PROOF_H
#define DEPTEST_PROOF_H

#include <solv/pool.h>
#include <solv/solver.h>
#include <solv/problems.h>
#include <solv/chksum.h>

static struct rclass2str {
  Id rclass;
  const char *str;
} rclass2str[] = {
  { SOLVER_RULE_PKG, "pkg" },
  { SOLVER_RULE_UPDATE, "update" },
  { SOLVER_RULE_FEATURE, "feature" },
  { SOLVER_RULE_JOB, "job" },
  { SOLVER_RULE_DISTUPGRADE, "distupgrade" },
  { SOLVER_RULE_INFARCH, "infarch" },
  { SOLVER_RULE_CHOICE, "choice" },
  { SOLVER_RULE_LEARNT, "learnt" },
  { SOLVER_RULE_BEST, "best" },
  { SOLVER_RULE_YUMOBS, "yumobs" },
  { SOLVER_RULE_BLACK, "black" },
  { SOLVER_RULE_RECOMMENDS, "recommends" },
  { SOLVER_RULE_STRICT_REPO_PRIORITY, "strictrepoprio" },
  { 0, 0 }
};

static const char *
testcase_rclass2str(Id rclass)
{
  int i;
  for (i = 0; rclass2str[i].str; i++)
    if (rclass == rclass2str[i].rclass)
      return rclass2str[i].str;
  return "unknown";
}

typedef struct strqueue {
  char **str;
  int nstr;
} Strqueue;

#define STRQUEUE_BLOCK 63

static void
strqueue_init(Strqueue *q)
{
  q->str = 0;
  q->nstr = 0;
}

static void
strqueue_free(Strqueue *q)
{
  int i;
  for (i = 0; i < q->nstr; i++)
    solv_free(q->str[i]);
  q->str = solv_free(q->str);
  q->nstr = 0;
}

static void
strqueue_push(Strqueue *q, const char *s)
{
  q->str = solv_extend(q->str, q->nstr, 1, sizeof(*q->str), STRQUEUE_BLOCK);
  q->str[q->nstr++] = solv_strdup(s);
}

static void
strqueue_pushjoin(Strqueue *q, const char *s1, const char *s2, const char *s3)
{
  q->str = solv_extend(q->str, q->nstr, 1, sizeof(*q->str), STRQUEUE_BLOCK);
  q->str[q->nstr++] = solv_dupjoin(s1, s2, s3);
}

static int
strqueue_sort_cmp(const void *ap, const void *bp, void *dp)
{
  const char *a = *(const char **)ap;
  const char *b = *(const char **)bp;
  return strcmp(a ? a : "", b ? b : "");
}

static void
strqueue_sort(Strqueue *q)
{
  if (q->nstr > 1)
    solv_sort(q->str, q->nstr, sizeof(*q->str), strqueue_sort_cmp, 0);
}

static void
strqueue_sort_u(Strqueue *q)
{
  int i, j;
  strqueue_sort(q);
  for (i = j = 0; i < q->nstr; i++)
    if (!j || strqueue_sort_cmp(q->str + i, q->str + j - 1, 0) != 0)
      q->str[j++] = q->str[i];
  q->nstr = j;
}

static char *
strqueue_join(Strqueue *q)
{
  int i, l = 0;
  char *r, *rp;
  for (i = 0; i < q->nstr; i++)
    if (q->str[i])
      l += strlen(q->str[i]) + 1;
  l++;	/* trailing \0 */
  r = solv_malloc(l);
  rp = r;
  for (i = 0; i < q->nstr; i++)
    if (q->str[i])
      {
        strcpy(rp, q->str[i]);
        rp += strlen(rp);
	*rp++ = '\n';
      }
  *rp = 0;
  return r;
}

static void
strqueue_split(Strqueue *q, const char *s)
{
  const char *p;
  if (!s)
    return;
  while ((p = strchr(s, '\n')) != 0)
    {
      q->str = solv_extend(q->str, q->nstr, 1, sizeof(*q->str), STRQUEUE_BLOCK);
      q->str[q->nstr] = solv_malloc(p - s + 1);
      if (p > s)
	memcpy(q->str[q->nstr], s, p - s);
      q->str[q->nstr][p - s] = 0;
      q->nstr++;
      s = p + 1;
    }
  if (*s)
    strqueue_push(q, s);
}

static void
strqueue_diff(Strqueue *sq1, Strqueue *sq2, Strqueue *osq)
{
  int i = 0, j = 0;
  while (i < sq1->nstr && j < sq2->nstr)
    {
      int r = strqueue_sort_cmp(sq1->str + i, sq2->str + j, 0);
      if (!r)
	i++, j++;
      else if (r < 0)
	strqueue_pushjoin(osq, "-", sq1->str[i++], 0);
      else
	strqueue_pushjoin(osq, "+", sq2->str[j++], 0);
    }
  while (i < sq1->nstr)
    strqueue_pushjoin(osq, "-", sq1->str[i++], 0);
  while (j < sq2->nstr)
    strqueue_pushjoin(osq, "+", sq2->str[j++], 0);
}

/**********************************************************/

const char *
testcase_solvid2str(Pool *pool, Id p)
{
  Solvable *s = pool->solvables + p;
  const char *n, *e, *a;
  char *str, buf[20];

  if (p == SYSTEMSOLVABLE)
    return "@SYSTEM";
  n = pool_id2str(pool, s->name);
  e = pool_id2str(pool, s->evr);
  a = pool_id2str(pool, s->arch);
  str = pool_alloctmpspace(pool, strlen(n) + strlen(e) + strlen(a) + 3);
  sprintf(str, "%s-%s", n, e);
  if (solvable_lookup_type(s, SOLVABLE_BUILDFLAVOR))
    {
      Queue flavorq;
      int i;

      queue_init(&flavorq);
      solvable_lookup_idarray(s, SOLVABLE_BUILDFLAVOR, &flavorq);
      for (i = 0; i < flavorq.count; i++)
	str = pool_tmpappend(pool, str, "-", pool_id2str(pool, flavorq.elements[i]));
      queue_free(&flavorq);
    }
  if (s->arch)
    str = pool_tmpappend(pool, str, ".", a);
  if (!s->repo)
    return pool_tmpappend(pool, str, "@", 0);
  if (s->repo->name)
    {
      int l = strlen(str);
      str = pool_tmpappend(pool, str, "@", s->repo->name);
      for (; str[l]; l++)
	if (str[l] == ' ' || str[l] == '\t')
	  str[l] = '_';
      return str;
    }
  sprintf(buf, "@#%d", s->repo->repoid);
  return pool_tmpappend(pool, str, buf, 0);
}

static const char *
testcase_ruleid(Solver *solv, Id rid)
{
  Strqueue sq;
  Queue q;
  int i;
  Chksum *chk;
  const unsigned char *md5;
  int md5l;
  const char *s;

  queue_init(&q);
  strqueue_init(&sq);
  solver_ruleliterals(solv, rid, &q);
  for (i = 0; i < q.count; i++)
    {
      Id p = q.elements[i];
      s = testcase_solvid2str(solv->pool, p > 0 ? p : -p);
      if (p < 0)
	s = pool_tmpjoin(solv->pool, "!", s, 0);
      strqueue_push(&sq, s);
    }
  queue_free(&q);
  strqueue_sort_u(&sq);
  chk = solv_chksum_create(REPOKEY_TYPE_MD5);
  for (i = 0; i < sq.nstr; i++)
    solv_chksum_add(chk, sq.str[i], strlen(sq.str[i]) + 1);
  md5 = solv_chksum_get(chk, &md5l);
  s = pool_bin2hex(solv->pool, md5, md5l);
  chk = solv_chksum_free(chk, 0);
  strqueue_free(&sq);
  return s;
}

static const char *
testcase_problemid(Solver *solv, Id problem)
{
  Strqueue sq;
  Queue q;
  Chksum *chk;
  const unsigned char *md5;
  int i, md5l;
  const char *s;

  /* we build a hash of all rules that define the problem */
  queue_init(&q);
  strqueue_init(&sq);
  solver_findallproblemrules(solv, problem, &q);
  for (i = 0; i < q.count; i++)
    strqueue_push(&sq, testcase_ruleid(solv, q.elements[i]));
  queue_free(&q);
  strqueue_sort_u(&sq);
  chk = solv_chksum_create(REPOKEY_TYPE_MD5);
  for (i = 0; i < sq.nstr; i++)
    solv_chksum_add(chk, sq.str[i], strlen(sq.str[i]) + 1);
  md5 = solv_chksum_get(chk, &md5l);
  s = pool_bin2hex(solv->pool, md5, 4);
  chk = solv_chksum_free(chk, 0);
  strqueue_free(&sq);
  return s;
}

char * doProof(Solver *solv)
{
  Pool *pool = solv->pool;
  int i, j;
  //Id p, op;
  const char *s;
  char *result;
  Strqueue sq;
  strqueue_init(&sq);
  {
    char *probprefix;
    int pcnt, problem;
    Queue q, lq;

    queue_init(&q);
    queue_init(&lq);
    pcnt = solver_problem_count(solv);
    for (problem = 1; problem <= pcnt + lq.count; problem++)
    {
      if (problem <= pcnt)
      {
        s = testcase_problemid(solv, problem);
        solver_get_proof(solv, problem, 0, &q);
      }
      else
      {
        s = testcase_ruleid(solv, lq.elements[problem - pcnt - 1]);
        solver_get_proof(solv, lq.elements[problem - pcnt - 1], 1, &q);
      }
      probprefix = solv_dupjoin("proof ", s, 0);
      for (i = 0; i < q.count; i += 2)
      {
        SolverRuleinfo rclass;
        Queue rq;
        Id rid = q.elements[i];
        char *rprefix;
        Id truelit = i + 1 < q.count ? q.elements[i + 1] : 0;
        char nbuf[16];

        rclass = solver_ruleclass(solv, rid);
        if (rclass == SOLVER_RULE_LEARNT)
          queue_pushunique(&lq, rid);
        queue_init(&rq);
        solver_ruleliterals(solv, rid, &rq);
        sprintf(nbuf, "%3d", i / 2);
        rprefix = solv_dupjoin(probprefix, " ", nbuf);
        rprefix = solv_dupappend(rprefix, " ", testcase_rclass2str(rclass));
        rprefix = solv_dupappend(rprefix, " ", testcase_ruleid(solv, rid));
        strqueue_push(&sq, rprefix);
        solv_free(rprefix);
        rprefix = solv_dupjoin(probprefix, " ", nbuf);
        rprefix = solv_dupappend(rprefix, ": ", 0);
        for (j = 0; j < rq.count; j++)
        {
          const char *s;
          Id p = rq.elements[j];
          if (p == truelit)
            s = pool_tmpjoin(pool, rprefix, "-->", 0);
          else
            s = pool_tmpjoin(pool, rprefix, "   ", 0);
          if (p < 0)
            s = pool_tmpappend(pool, s, " -", testcase_solvid2str(pool, -p));
          else
            s = pool_tmpappend(pool, s, "  ", testcase_solvid2str(pool, p));
          strqueue_push(&sq, s);
        }
        solv_free(rprefix);
        queue_free(&rq);
      }
      solv_free(probprefix);
    }
    queue_free(&q);
    queue_free(&lq);
  }
  strqueue_sort(&sq);
  result = strqueue_join(&sq);
  strqueue_free(&sq);
  return result;
}

#endif // DEPTEST_PROOF_H
