import io.infinit.State;
import io.infinit.User;
import io.infinit.PeerTransaction;
import io.infinit.LinkTransaction;
import io.infinit.TransactionStatus;
import java.io.FileOutputStream;

public class Test extends State
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
    byte[] data = avatar(id);
    User u = userById(id);
    if (data.length == 0)
      return;
    String path = "/tmp/avatar-" + u.fullname + ".jpg";
    FileOutputStream fos;
    try {
      fos = new FileOutputStream(path);
      fos.write(data);
      fos.close();
    }
    catch(Exception e)
    {}
  }
  public void onConnection(boolean isConnected, boolean stillTrying, String lastError) {
    System.err.printf("connection connected=%s retry=%s error=%s\n", isConnected, stillTrying, lastError);
    if (isConnected)
    {
      processArgs();
    }
  }
  public void onPeerTransaction(PeerTransaction transaction) {
    User u = userById(transaction.senderId);
    System.err.printf("Peer transaction from %s, status=%s\n", u.fullname, transaction.status);
    if (transaction.status == TransactionStatus.WAITING_ACCEPT && transaction.senderId != selfId())
    {
      System.err.printf("accepting...\n");
      acceptTransaction(transaction.id);
    }
  }
  public void onLink(LinkTransaction link) {
    System.err.printf("Link(%s) transaction status = %s\n", link.id, link.status);
    if (!link.link.equals(""))
    {
      System.out.printf("Your link is: %s\n", link.link);
    }
  }
  String[] args;

  void processArgs()
  {
    System.err.printf("processing actions\n");
    if (args.length < 3)
      return;
    if (args[2].equals("s"))
    {
      String target = args[3];
      User u = userByEmail(target);

      if (u.fullname.equals(""))
      {
        u = userByHandle(target);
        if (u.fullname.equals(""))
        {
          User[] users = usersSearch(target);
          if (users.length == 1)
            u = users[0];
          else
          {
            System.err.printf("%s candidates, aborting\n", users.length);
            return;
          }
        }
      }
      System.err.printf("Got user %s\n", u.fullname);
      int i = sendFiles(u.id, args[4].split(";"), args[5]);
      System.err.printf("Created transaction %s\n", i);
    }
    else if (args[2].equals("l"))
    {
      int i = createLinkTransaction(args[3].split(";"), args[4]);
    }
    args = new String[]{};
  }
  public static void main(String[] args)
  {
    Test t = new Test();
    t.args = args;
    // FIXME: replace with writable directories (see initialize() signature)
    t.initialize(true, "/tmp", "/tmp", "/tmp", true, 0);
    t.login(args[0], args[1]);
    System.out.println("Logging in...");
    // delay doing anything until we are connected
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