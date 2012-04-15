/**
 *
 */
public class WorkloadGenerator {

  private static int randRange(int low, int high) {
    return low + (int)(Math.random() * ((high - low) + 1));
  }

  public static Query[] randomWorkload(int size, int maxBlocks, int maxDeadline) {
    Query[] workload = new Query[size];
    for (int i = 0; i < workload.length; i++) {
      int blocks = randRange(1, maxBlocks);
      int deadline = randRange(1, maxDeadline);
      workload[i] = new Query(blocks, deadline);
    }
    return workload;
  }
}
