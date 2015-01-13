
package io.infinit;

import java.util.Map;
import java.util.HashMap;
import java.lang.RuntimeException;
import java.lang.Integer;



public class State
{
  public void initialize(boolean production,
    String download_dir, String persistent_config_dir,
    String non_persistent_config_dir,
    boolean enable_mirroring,
    long max_mirroring_size)
  {
    handle = gapInitialize(production, download_dir, persistent_config_dir,
      non_persistent_config_dir, enable_mirroring, max_mirroring_size);
  }
  public void login(String email, String hashed_password)
  {
    long res = gapLogin(handle, email, hashed_password);
    _check("login", res);
  }
  public void register(String fullname, String email, String hashed_password)
  {
    long res = gapRegister(handle, fullname, email, hashed_password);
    _check("register", res);
  }
  public void setProxy(int type, String host, int port, String username, String password)
  {
    long res = gapSetProxy(handle, type, host, port, username, password);
    _check("setProxy", res);
  }
  public void unsetProxy(int type)
  {
    long res = gapUnsetProxy(handle, type);
    _check("unsetProxy", res);
  }
  public void cleanState()
  {
    gapCleanState(handle);
  }
  public HashMap features()
  {
    return gapFetchFeatures(handle);
  }
  public boolean loggedIn()
  {
    return gapLoggedIn(handle);
  }
  public void logout()
  {
    long res = gapLogout(handle);
    _check("logout", res);
  }
  public PeerTransaction peerTransactionById(int id)
  {
    return gapPeerTransactionById(handle, id);
  }
  public LinkTransaction linkTransactionById(int id)
  {
    return gapLinkTransactionById(handle, id);
  }
  public float transactionProgress(int id)
  {
    return gapTransactionProgress(handle, id);
  }
  public boolean transactionIsFinal(int id)
  {
    return gapTransactionIsFinal(handle, id);
  }
  public boolean transactionConcernDevice(int id)
  {
    return gapTransactionConcernDevice(handle, id);
  }
  public void poll()
  {
    long res = gapPoll(handle);
    _check("poll", res);
  }
  public long deviceStatus()
  {
    return gapDeviceStatus(handle);
  }
  public void setDeviceName(String name)
  {
    long res = gapSetDeviceName(handle, name);
    _check("set device name", res);
  }
  public String selfEmail()
  {
    return gapSelfEmail(handle);
  }
  public void setEmail(String email, String password)
  {
    long ret = gapSetSelfEmail(handle, email, password);
    _check("set email", ret);
  }
  public String selfFullname()
  {
    return gapSelfFullname(handle);
  }
  public void setSelfFullname(String name)
  {
    long ret = gapSetSelfFullname(handle, name);
    _check("set fullname", ret);
  }
  public String selfHandle()
  {
    return gapSelfHandle(handle);
  }
  public void setSelfHandle(String name)
  {
    long ret = gapSetSelfHandle(handle, name);
    _check("set fullname", ret);
  }
  public void changePassword(String old, String password)
  {
    long ret = gapChangePassword(handle, old, password);
    _check("change password", ret);
  }
  public long selfId()
  {
    return gapSelfId(handle);
  }
  public void updateAvatar(byte[] data)
  {
    long ret = gapUpdateAvatar(handle, data);
    _check("update avatar", ret);
  }
  public String selfDeviceId()
  {
    return gapSelfDeviceId(handle);
  }
  public byte[] avatar(int id)
  {
    return gapAvatar(handle, id);
  }
  public void refreshAvatar(int id)
  {
    gapRefreshAvatar(handle, id);
  }
  public User userById(int id)
  {
    return gapUserById(handle, id);
  }
  public User userByEmail(String email)
  {
    return gapUserByEmail(handle, email);
  }
  public User userByHandle(String h)
  {
    return gapUserByHandle(handle, h);
  }
  public long userStatus(int id)
  {
    return gapUserStatus(handle, id);
  }
  public User[] usersSearch(String query)
  {
    return gapUsersSearch(handle, query);
  }
  public User[] swaggers()
  {
    return gapSwaggers(handle);
  }
  public int[] favorites()
  {
    return gapFavorites(handle);
  }
  public long favorite(int id)
  {
    return gapFavorite(handle, id);
  }
  public long unFavorite(int id)
  {
    return gapUnfavorite(handle, id);
  }
  public boolean isFavorite(int id)
  {
    return gapIsFavorite(handle, id);
  }
  public boolean isLinkTransaction(int id)
  {
    return gapIsLinkTransaction(handle, id);
  }
  public int createLinkTransaction(String[] files, String message)
  {
    int res = gapCreateLinkTransaction(handle, files, message);
    _checkNZ("create link transaction", res);
    return res;
  }
  public LinkTransaction[] linkTransactions()
  {
    return gapLinkTransactions(handle);
  }
  public PeerTransaction[] peerTransactions()
  {
    return gapPeerTransactions(handle);
  }
  public int sendFiles(int id, String[] files, String message)
  {
    int res = gapSendFiles(handle, id, files, message);
    _checkNZ("send files", res);
    return res;
  }
  public int sendFilesByEmail(String id, String[] files, String message)
  {
    int res = gapSendFilesByEmail(handle, id, files, message);
    _checkNZ("send files to mail", res);
    return res;
  }
  public int pauseTransaction(int id)  { int res = gapPauseTransaction(handle, id); _checkNZ("state change", res); return res;}
  public int resumeTransaction(int id) { int res = gapResumeTransaction(handle, id);_checkNZ("state change", res); return res;}
  public int cancelTransaction(int id) { int res = gapCancelTransaction(handle, id);_checkNZ("state change", res); return res;}
  public int deleteTransaction(int id) { int res = gapDeleteTransaction(handle, id);_checkNZ("state change", res); return res;}
  public int rejectTransaction(int id) { int res = gapRejectTransaction(handle, id);_checkNZ("state change", res); return res;}
  public int acceptTransaction(int id) { int res = gapAcceptTransaction(handle, id);_checkNZ("state change", res); return res;}

  public class Onboarding
  {
    public Onboarding(State ctx, int idx)
    {
      context = ctx;
      idx = id;
    }
    public void setPeerStatus(boolean s)
    {
      context.onboardingSetPeerStatus(id, s);
    }
    public void setPeerAvailability(boolean s)
    {
      context.onboardingSetPeerAvailability(id, s);
    }
    private State context;
    private int id;
  }
  public Onboarding newOnboarding(String path, int duration_sec)
  {
    return new Onboarding(this, gapOnboardingReceiveTransaction(handle, path, duration_sec));
  }
  public void onboardingSetPeerStatus(int id, boolean s)
  {
    long res = gapOnboardingSetPeerStatus(handle, id, s);
    _check("onboarding peer status", res);
  }
  public void onboardingSetPeerAvailability(int id, boolean s)
  {
    long res = gapOnboardingSetPeerAvailability(handle, id, s);
    _check("onboarding peer status", res);
  }

  public void setOutputDir(String path, boolean fallback)
  {
    long res = gapSetOutputDir(handle, path, fallback);
    _check("set output dir", res);
  }
  public String outputDir()
  {
    return gapGetOutputDir(handle);
  }
  public void sendMetric(long metricId, HashMap<String, String> extra)
  {
    String[] extras = new String[extra.size()*2];
    int p = 0;
    for (Map.Entry<String, String> entry : extra.entrySet()) {
      String key = entry.getKey();
      String value = entry.getValue();
      extras[p] = key;
      extras[p+1] = value;
      p+= 2;
    }
    long res = gapSendMetric(handle, metricId, extras);
    _check("send metric", res);
  }
  public void sendUserReport(String userName, String message, String file)
  {
    long res = gapSendUserReport(handle, userName, message, file);
    _check("send user report", res);
  }
  public void sendLastCrashLogs(String userName, String crashReport,
                                String stateLog, String extra)
  {
    long res = gapSendLastCrashLogs(handle, userName, crashReport, stateLog, extra);
    _check("send crash logs", res);
  }
  public void internetConnection(boolean connected)
  {
    long res = gapInternetConnection(handle, connected);
    _check("internet connection", res);
  }

  /// Callbacks
  public void onCritical() {}
  public void onNewSwagger(User user) {}
  public void onDeletedSwagger(int id) {}
  public void onDeletedFavorite(int id) {}
  public void onUserStatus(int id, boolean status) {}
  public void onAvatarAvailable(int id)  {}
  public void onConnection(boolean isConnected, boolean stillTrying, String lastError) {}
  public void onPeerTransaction(PeerTransaction transaction) {}
  public void onLink(LinkTransaction link) {}

  protected
  void finalize()
  {
    if (handle != 0)
      gapFinalize(handle);
    handle = 0;
  }
  private long handle = 0;
  private void _check(String op, long res)
  {
    if (res != 1)
      throw new RuntimeException(op + " failed with " + new Long(res).toString());
  }
  private void _checkNZ(String op, int res)
  {
    if (res == 0)
      throw new RuntimeException(op + " failed");
  }
  private native long gapInitialize(
    boolean production,
    String download_dir, String persistent_config_dir,
    String non_persistent_config_dir,
    boolean enable_mirroring,
    long max_mirroring_size);
  private native long gapFinalize(long handle);
  private native long gapLogin(long handle, String email, String hashed_password);
  private native long gapRegister(long handle, String fullname, String email, String hashed_password);
  private native long gapSetProxy(long handle, int type, String host, int port, String username, String password);
  private native long gapUnsetProxy(long handle, int type);
  private native long gapCleanState(long handle);
  private native HashMap gapFetchFeatures(long handle);
  private native boolean gapLoggedIn(long handle);
  private native long gapLogout(long handle);
  private native PeerTransaction gapPeerTransactionById(long handle, int id);
  private native float gapTransactionProgress(long handle, int id);
  private native boolean gapTransactionIsFinal(long handle, int id);
  private native boolean gapTransactionConcernDevice(long handle, int id);
  private native long gapPoll(long handle);
  private native long gapDeviceStatus(long handle);
  private native long gapSetDeviceName(long handle, String name);
  private native String gapSelfEmail(long handle);
  private native long gapSetSelfEmail(long handle, String mail, String password);
  private native String gapSelfFullname(long handle);
  private native long gapSetSelfFullname(long handle, String name);
  private native String gapSelfHandle(long handle);
  private native long gapSetSelfHandle(long handle, String name);
  private native long gapChangePassword(long handle, String old, String pass);
  private native long gapSelfId(long handle);
  private native long gapUpdateAvatar(long handle, byte[] data);
  private native String gapSelfDeviceId(long handle);
  private native byte[] gapAvatar(long handle, int id);
  private native void gapRefreshAvatar(long handle, int id);
  private native User gapUserById(long handle, int id);
  private native User gapUserByEmail(long handle, String email);
  private native User gapUserByHandle(long handle, String email);
  private native long gapUserStatus(long handle, int id);
  private native User[] gapUsersSearch(long handle, String query);
  private native User[] gapSwaggers(long handle);
  private native int[] gapFavorites(long handle);
  private native long gapFavorite(long handle, int id);
  private native long gapUnfavorite(long handle, int id);
  private native boolean gapIsFavorite(long handle, int id);
  private native boolean gapIsLinkTransaction(long handle, int id);
  private native int gapCreateLinkTransaction(long handle, String[] files,
                                               String message);
  private native LinkTransaction gapLinkTransactionById(long handle, int id);
  private native LinkTransaction[] gapLinkTransactions(long handle);
  private native PeerTransaction[] gapPeerTransactions(long handle);
  private native int gapSendFiles(long handle, int id, String[] files, String message);
  private native int gapSendFilesByEmail(long handle, String image, String[] files, String message);
  private native int gapPauseTransaction(long handle, int id);
  private native int gapResumeTransaction(long handle, int id);
  private native int gapCancelTransaction(long handle, int id);
  private native int gapDeleteTransaction(long handle, int id);
  private native int gapRejectTransaction(long handle, int id);
  private native int gapAcceptTransaction(long handle, int id);

  private native int gapOnboardingReceiveTransaction(long handle, String path, int transfer_time_sec);
  private native long gapOnboardingSetPeerStatus(long handle, int id, boolean status);
  private native long gapOnboardingSetPeerAvailability(long handle, int id, boolean status);

  private native long gapSetOutputDir(long handle, String path, boolean fallback);
  private native String gapGetOutputDir(long handle);

  private native long gapSendMetric(long handle, long metric, String[] extra);
  private native long gapSendUserReport(long handle, String userName, String message, String file);
  private native long gapSendLastCrashLogs(long handle, String userName, String crashReport, String stateLog, String extraInfo);
  private native long gapInternetConnection(long handle, boolean connected);
  /* this is used to load the 'hello-jni' library on application
  * startup. The library has already been unpacked into
  * /data/data/com.example.hellojni/lib/libhello-jni.so at
  * installation time by the package manager.
  */
  static {
    System.loadLibrary("jniinfinit");
  }
}