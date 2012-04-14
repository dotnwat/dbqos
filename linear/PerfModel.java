import java.io.*;

/**
 * Simple method for reading up performance model information to implement
 * t_S, t_I, and t_Is look-up functions.
 *
 * File format:
 *   line2: iops_S array
 *   line3: iops_I array
 *   line4: iops_Is array
 */
public class PerfModel {

  private double[] iops_S;
  private double[] iops_I;
  private double[] iops_Is;

  /**
   * Parse line into array of doubles
   */
  private double[] parseToDoubles(String line) {
    String[] tokens = line.split(" ");
    double[] arr = new double[tokens.length];
    for (int i = 0; i < tokens.length; i++)
      arr[i] = Double.parseDouble(tokens[i]);
    return arr;
  }

  /**
   *
   */
  public PerfModel(String filename) throws IOException {
    FileInputStream fis = new FileInputStream(filename);
    DataInputStream dis = new DataInputStream(fis);
    BufferedReader br = new BufferedReader(new InputStreamReader(dis));

    String line = br.readLine();
    iops_S = parseToDoubles(line);

    line = br.readLine();
    iops_I = parseToDoubles(line);

    line = br.readLine();
    iops_Is = parseToDoubles(line);

    if (br.readLine() != null)
      throw new IOException("Too many arrays found. Invalid file.");

    if (iops_S.length != iops_I.length || iops_S.length != iops_Is.length)
      throw new IOException("Arrays not equally sized. Invalid file.");
  }

  /**
   * Time to complete query using a sequential scan for a given number of
   * concurrent index scans.
   *
   * @param q The query
   * @param n Number of index scans
   */
  public double t_S(Query q, int n) {
    if (iops_S[n] == 0)
      return -1000000.0;
    return ((double)q.getBlocks()) / iops_S[n];
  }

  /**
   * Time to complete query using an index scan with zero concurrent
   * sequential scans.
   *
   * @param q The query
   * @param n Number of index scans
   */
  public double t_I(Query q, int n) {
    if (iops_I[n] == 0)
      return -1000000.0;
    return ((double)q.getBlocks()) / iops_I[n];
  }

  /**
   * Time to complete query using an index scan with one concurrent
   * sequential scans.
   *
   * @param q The query
   * @param n Number of index scans
   */
  public double t_Is(Query q, int n) {
    if (iops_Is[n] == 0)
      return -1000000.0;
    return ((double)q.getBlocks()) / iops_Is[n];
  }

}
