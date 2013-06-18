//
// ---------- header ----------------------------------------------------------
//
// project       user
//
// license       infinit
//
// author        julien quintard   [sat mar 27 08:37:14 2010]
//

#ifndef USER_USER_HH
#define USER_USER_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/types.hh>

namespace satellite
{

//
// ---------- classes ---------------------------------------------------------
//

  ///
  /// this class implements the user satellite.
  ///
  class User
  {
  public:
    //
    // enumerations
    //
    enum Operation
      {
        OperationUnknown = 0,

        OperationCreate,
        OperationDestroy,
        OperationInformation
      };

    //
    // static methods
    //
    static elle::Status         Create(elle::String const& id,
                                       const elle::String&,
                                       elle::String const& authority_path);
    static elle::Status         Destroy(const elle::String&);
    static elle::Status         Information(const elle::String&);
  };

}

#endif
