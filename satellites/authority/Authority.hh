#ifndef AUTHORITY_AUTHORITY_HH
#define AUTHORITY_AUTHORITY_HH

//
// ---------- includes --------------------------------------------------------
//

#include <Infinit.hh>
#include <elle/types.hh>
#include <lune/Lune.hh>
#include <etoile/Etoile.hh>

namespace satellite
{

//
// ---------- classes ---------------------------------------------------------
//

  ///
  /// this class implements the authority satellite.
  ///
  class Authority
  {
  public:
    //
    // constants
    //
    static const elle::Natural32                Length;

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
    static elle::Status         Create(elle::String const& authority_path);
    static elle::Status         Destroy(elle::String const& authority_path);
    static elle::Status         Information(elle::String const& authority_path);
  };

}

#endif
