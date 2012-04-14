/**
 * Information about a query.
 */
public class Query {

  private int blocks;
  private int deadline;

  public Query(int blocks, int deadline) {
    this.blocks = blocks;
    this.deadline = deadline;
  }

  public int getBlocks() {
    return blocks;
  }

  public int getDeadline() {
    return deadline;
  }

}
