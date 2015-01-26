
package io.infinit;

import java.util.Map;
import java.util.HashMap;
import java.lang.RuntimeException;
import java.lang.Integer;


public class StateException extends java.lang.RuntimeException
{
  String operation;
  GapStatus errorCode;
  StateException(String op, GapStatus code)
  {
    super("operation '" + op + "' failed with " + code.toString());
    operation = op;
    errorCode = code;
  }
  StateException(String op, int code)
  {
    super("operation '" + op + "' failed with " + GapStatus.GetValue(code).toString());
    operation = op;
    errorCode = GapStatus.GetValue(code);
  }
  StateException(int code)
  {
    super("state exception: " + GapStatus.GetValue(code).toString());
    errorCode = GapStatus.GetValue(code);
  }
}