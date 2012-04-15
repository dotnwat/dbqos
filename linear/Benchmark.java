/**
 *
 */
import java.io.IOException;

/**
 *
 */
public class Benchmark {

  public static void main(String[] args) {

    Query[] workload = new Query[2];
    for (int i = 0; i < workload.length; i++)
      workload[i] = new Query(1, 0);

    PerfModel pm = null;
    try {
      pm = new PerfModel(args[0]);
    } catch (IOException e) {
      System.err.println(e);
      System.exit(-1);
    }

    GoodnessProblem gp = new GoodnessProblem(workload, pm);
    gp.solve(null);
  }

}
