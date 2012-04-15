/**
 *
 */
import java.io.IOException;

/**
 * Allow any size workload.
 */
public class PerfModelNoBounds extends PerfModel {

  public PerfModelNoBounds(String filename) throws IOException {
    super(filename);
  }

  /**
   * Time to complete query using a sequential scan for a given number of
   * concurrent index scans, and uses the max available if n exceeds the
   * bounds.
   *
   * @param q The query
   * @param n Number of index scans
   */
  public double t_S(Query q, int n) {
    try {
      return super.t_S(q, n);
    } catch (ArrayIndexOutOfBoundsException e) {
      return super.t_S(q, iops_S.length - 1);
    }
  }

  /**
   * Time to complete query using an index scan with zero concurrent
   * sequential scans, and uses the max available if n exceeds the array
   * bounds.
   *
   * @param q The query
   * @param n Number of index scans
   */
  public double t_I(Query q, int n) {
    try {
      return super.t_I(q, n);
    } catch (ArrayIndexOutOfBoundsException e) {
      return super.t_I(q, iops_I.length - 1);
    }
  }

  /**
   * Time to complete query using an index scan with one concurrent
   * sequential scans, and uses the max available if n exceeds the array
   * bounds.
   *
   * @param q The query
   * @param n Number of index scans
   */
  public double t_Is(Query q, int n) {
    try {
      return super.t_Is(q, n);
    } catch (ArrayIndexOutOfBoundsException e) {
      return super.t_Is(q, iops_Is.length - 1);
    }
  }
}
