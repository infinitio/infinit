#ifndef AGENT_AGENT_HH
# define AGENT_AGENT_HH

#include <Infinit.hh>
#include <elle/types.hh>
#include <infinit/Identity.hh>
#include <nucleus/neutron/Subject.hh>

///
/// the agent namespace contains everything related to the agent
/// component i.e the entity responsible for performing cryptographic
/// operations on behalf of the user.
///
namespace agent
{

  ///
  /// this class implements the agent by providing an entry point
  /// to user-specific cryptographic functionalities.
  ///
  /// this class comes with some information such as the user identity,
  /// and a ready-to-use subject.
  ///
  class Agent
  {
  public:
    //
    // static methods
    //
    static elle::Status         Initialize();
    static elle::Status         Clean();

    /*---------------.
    | Static Methods |
    `---------------*/
  public:
    /// Return the identity associated with the current user.
    static
    infinit::Identity const&
    identity();
    /// Return the key pair associated with the current user.
    static
    cryptography::KeyPair const&
    keypair();
    /// Return the subject referencing the current user: useful for
    /// managing access credentials.
    static
    nucleus::neutron::Subject const&
    subject();

    //
    // static attributes
    //
    static elle::String meta_token;
    static elle::String identity_passphrase;
  };

}

#endif
