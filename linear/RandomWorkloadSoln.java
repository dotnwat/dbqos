/**
 *
 */
import java.io.IOException;

/**
 * Generates, solves, and prints the solution to random workloads.
 *
 * This is used in conjunction with a Python-based exhaustive search utility
 * to sanity check the correctness of the BIP implementation.
 */
public class RandomWorkloadSoln {

  /**
   * Create an easily parsed representation of the workload.
   *
   * This string can be directly eval'd in Python :)
   */
  public static String writeWorkload(Query[] workload) {
    String r = "(";
    for (int i = 0; i < workload.length; i++) {
      Query q = workload[i];
      r += "(" + q.getBlocks() + ", " + q.getDeadline() + ")";
      if ((i+1) < workload.length)
        r += ", ";
    }
    r += ")";
    return r;
  }

  /**
   *
   */
  public static void solveProblem(Query[] workload, PerfModel pm) {
    GoodnessProblem gp = new GoodnessProblem(workload, pm);
    gp.solve("out.lp");
    gp.printSolutionMatrix();
  }

  /**
   *
   */
  public static void main(String[] args) {
    PerfModel pm = null;
    try {
      pm = new PerfModelNoBounds(args[0]);
    } catch (IOException e) {
      System.err.println(e);
      System.exit(-1);
    }

    int maxWorkloadSize = pm.getMaxSize();
    int maxblk = 262144;
    int maxlat = 300;

    while (true) {
      int size = WorkloadGenerator.randRange(1, maxWorkloadSize);
      Query[] workload = WorkloadGenerator.randomWorkload(size, maxblk, maxlat);
      //Query[] workload = new Query[1];
      //workload[0] = new Query(262144, 300);
      solveProblem(workload, pm);
      break;
    }
  }
}
