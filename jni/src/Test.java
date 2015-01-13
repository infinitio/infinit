import io.infinit.State;
import io.infinit.User;
import io.infinit.PeerTransaction;
import io.infinit.LinkTransaction;
class Test extends State
{
  public void onCritical() {
    System.out.print("CRITICAL");
  }
  public void onNewSwagger(User user) {
    System.err.printf("new swagger %s\n", user.fullname);
  }
  public void onDeletedSwagger(int id) {
    System.err.printf("delete swagger %s\n", id);
  }
  public void onDeletedFavorite(int id) {
    System.err.printf("delete favorite %s\n", id);
  }
  public void onUserStatus(int id, boolean status) {
    System.err.printf("user status %s %s\n", id, status);
  }
  public void onAvatarAvailable(int id)  {
    System.err.printf("avatar %s\n", id);
  }
  public void onConnection(boolean isConnected, boolean stillTrying, String lastError) {
    System.err.printf("connection %s %s %s\n", isConnected, stillTrying, lastError);
  }
  public void onPeerTransaction(PeerTransaction transaction) {}
  public void onLink(LinkTransaction link) {}
  public static void main(String[] args)
  {
    Test t = new Test();
    t.initialize(true, "/tmp", "/tmp", "/tmp", true, 0);
    t.login(args[0], args[1]);
    System.out.println("Logged in");
    while (true)
    {
      t.poll();
      try {
        Thread.sleep(200);
      } catch (InterruptedException ie) {
        //Handle exception
      }
    }
  }
}