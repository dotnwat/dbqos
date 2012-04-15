/**
 *
 */
import ilog.concert.*;
import ilog.cplex.*;

/**
 *
 */
public class GoodnessProblem {

  private Query[] workload;
  private PerfModel pm;
  private long solvems = -1;

  public GoodnessProblem(Query[] workload, PerfModel pm) {
    this.workload = workload;
    this.pm = pm;
  }

  /**
   * Construct matrix of binary variables (s_qn, i_qn, is_qn).
   *
   * The first dimension indexes the query id [0..nQueries-1]. The second
   * dimension indexes the size of the index-scan partition [0..nQueries].
   *
   * @param prefix Variable name prefix
   * @param model The CPLEX model to use
   */
  private IloIntVar[][] buildPartitionVariables(String prefix,
      IloModeler model) throws IloException {

    IloIntVar[][] mat = new IloIntVar[workload.length][];
    for (int i = 0; i < workload.length; i++) {
      mat[i] = new IloIntVar[workload.length + 1];
      for (int j = 0; j <= workload.length; j++)
        mat[i][j] = model.boolVar(prefix + "(" + i + "," + j + ")");
    }

    return mat;
  }

  private void setupModel(IloModeler model) throws IloException {
    /* Setup vars for query partition choices */
    IloIntVar[][] s_qn = buildPartitionVariables("s", model);
    IloIntVar[][] i_qn = buildPartitionVariables("i", model);
    IloIntVar[][] is_qn = buildPartitionVariables("is", model);

    /* Setup z_n vars for restricting 'choice' of |QI| size */
    IloIntVar[] z_n = new IloIntVar[workload.length + 1];
    for (int j = 0; j <= workload.length; j++)
      z_n[j] = model.boolVar("z(" + j + ")");

    /*
     * Setup constraint: one partition selection per query
     *   sum_{n} i_qn + is_qn + s_qn = 1 forall q
     */
    for (int i = 0; i < workload.length; i++) {
      IloIntExpr expr = model.sum(model.sum(s_qn[i]),
          model.sum(i_qn[i]), model.sum(is_qn[i]));
      model.addEq(1.0, expr, "onePart(" + i + ")");
    }

    /*
     * Setup constraint: each query selects same |Qi| = n
     *   s_qn + i_qn + is_qn <= z_n forall n
     *   sum_{n} z_n = 1
     */
    for (int j = 0; j <= workload.length; j++) {
      for (int i = 0; i < workload.length; i++) {
        IloLinearIntExpr expr = model.linearIntExpr();
        expr.addTerm(s_qn[i][j], 1);
        expr.addTerm(i_qn[i][j], 1);
        expr.addTerm(is_qn[i][j], 1);
        model.addLe(expr, z_n[j], "oneRow(" + j + ")");
      }
    }
    model.addEq(1.0, model.sum(z_n), "sameRow");

    /*
     * Setup objective function
     *   sum_{n} sum_{q,t in Q} s_qn (t - t_S(q,n)) + i_qn (t-t_I(q,n)) +
     *     is_qn (t-t_Is(q,n))
     */
    IloLinearNumExpr goodness = model.linearNumExpr();
    for (int j = 0; j <= workload.length; j++) {
      for (int i = 0; i < workload.length; i++) {
        Query query = workload[i];
        goodness.addTerm(s_qn[i][j], query.getDeadline() - pm.t_S(query, j));
        goodness.addTerm(i_qn[i][j], query.getDeadline() - pm.t_I(query, j));
        goodness.addTerm(is_qn[i][j], query.getDeadline() - pm.t_Is(query, j));
      }
    }

    /* Add objective function to model */
    model.addMaximize(goodness);
  }

  public void solve(String out) {
    try {
      IloCplex cplex = new IloCplex();
      setupModel(cplex);
      if (out != null)
        cplex.exportModel(out);
      cplex.setOut(null);
      long cur = System.currentTimeMillis();
      cplex.solve();
      solvems = System.currentTimeMillis() - cur;
      cplex.end();
    } catch (IloException e) {
      e.printStackTrace();
      System.err.println("Caught CPLEX exception: " + e);
    }
  }

  public long getSolveMillis() {
    return solvems;
  }

}
