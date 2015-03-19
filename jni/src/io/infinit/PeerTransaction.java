
package io.infinit;

public class PeerTransaction
{
  public int id;
  public TransactionStatus status;
  public int senderId;
  public String senderDeviceId;
  public int recipientId;
  public String recipientDeviceId;
  public double mtime;
  public String[] fileNames;
  public long totalSize;
  public boolean isDirectory;
  public String message;
  public String metaId;
}