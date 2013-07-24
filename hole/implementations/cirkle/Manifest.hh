#ifndef HOLE_IMPLEMENTATIONS_CIRKLE_MANIFEST_HH
#define HOLE_IMPLEMENTATIONS_CIRKLE_MANIFEST_HH

#include <elle/types.hh>
#include <elle/Manifest.hh>

#include <lune/fwd.hh>

#include <hole/implementations/slug/Manifest.hh>
#include <hole/implementations/cirkle/fwd.hh>

//
// ---------- constants -------------------------------------------------------
//

namespace hole
{
  namespace implementations
  {
    namespace cirkle
    {

      ///
      /// XXX
      ///
      extern const elle::Character      Component[];

      ///
      /// XXX
      ///
      const elle::Natural32             Tags = 30;

    }
  }
}

//
// ---------- range -----------------------------------------------------------
//

///
/// XXX
///
range(hole::implementations::cirkle::Component,
      hole::implementations::cirkle::Tags,
      hole::implementations::slug::Component);

//
// ---------- tags ------------------------------------------------------------
//

namespace hole
{
  namespace implementations
  {
    namespace cirkle
    {

      //
      // enumerations
      //
      enum Tag
        {
          TagChallenge = elle::network::Range<Component>::First + 1,
          TagPassport,
          TagPort,
          TagAuthenticated,

          TagUpdate,

          TagPush,
          TagPull,
          TagBlock,
          TagWipe
        };

    }
  }
}

//
// ---------- manifests -------------------------------------------------------
//

///
/// below are the definitions of the inward and outward messages.
///

message(hole::implementations::cirkle::TagChallenge,
        parameters())
message(hole::implementations::cirkle::TagPassport,
        parameters(papier::Passport))
message(hole::implementations::cirkle::TagPort,
        parameters(elle::network::Port))
message(hole::implementations::cirkle::TagAuthenticated,
        parameters())

message(hole::implementations::cirkle::TagUpdate,
        parameters(hole::implementations::cirkle::Cluster))

message(hole::implementations::cirkle::TagPush,
        parameters(nucleus::Address,
                   nucleus::Derivable<nucleus::Block>));
message(hole::implementations::cirkle::TagPull,
        parameters(nucleus::Address,
                   nucleus::Version));
message(hole::implementations::cirkle::TagBlock,
        parameters(nucleus::Derivable<nucleus::Block>));
message(hole::implementations::cirkle::TagWipe,
        parameters(nucleus::Address));

#endif
