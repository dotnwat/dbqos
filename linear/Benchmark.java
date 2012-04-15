/**
 *
 */
import java.io.IOException;

/**
 *
 */
public class Benchmark {

  public static void main(String[] args) {

    PerfModel pm = null;
    try {
      pm = new PerfModelNoBounds(args[0]);
    } catch (IOException e) {
      System.err.println(e);
      System.exit(-1);
    }

    int maxSize = 50;
    int repeat = 3;

    for (int i = 1; i <= maxSize; i++) {
      Query[] workload = WorkloadGenerator.randomWorkload(i, 262144, 300);
      long elapsed = 0;
      for (int j = 0; j < repeat; j++) {
        GoodnessProblem gp = new GoodnessProblem(workload, pm);
        gp.solve(null);
        elapsed += gp.getSolveMillis();
      }
      String out = i + " " + (elapsed / (float)repeat);
      System.out.println(out);
    }
  }

}
