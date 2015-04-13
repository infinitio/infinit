
package io.infinit;

import java.lang.RuntimeException;

public enum TransactionStatus
{
  NEW(0),
  ON_OTHER_DEVICE(1),
  WAITING_ACCEPT(2),
  WAITING_DATA(3),
  CONNECTING(4),
  TRANSFERRING(5),
  CLOUD_BUFFERED(6),
  FINISHED(7),
  FAILED(8),
  CANCELED(9),
  REJECTED(10),
  DELETED(11),
  PAUSED(12);
  int id;
  private TransactionStatus(int i){id = i;}
  public boolean Compare(int i){return id == i;}
  public static TransactionStatus GetValue(int _id)
  {
    TransactionStatus[] vals = TransactionStatus.values();
    for(int i = 0; i < vals.length; i++)
    {
      if(vals[i].Compare(_id))
        return vals[i];
    }
    throw new RuntimeException("id not in range");
  }
}