#include <agent/Agent.hh>

#include <elle/io/Console.hh>
#include <elle/serialize/extract.hh>

#include <lune/Lune.hh>

#include <common/common.hh>

#include <infinit/Identity.hh>

#include <Infinit.hh>

#include <boost/filesystem.hpp>

#include <fstream>

namespace agent
{

//
// ---------- definitions -----------------------------------------------------
//

  elle::String Agent::meta_token;
  elle::String Agent::identity_passphrase;

//
// ---------- methods ---------------------------------------------------------
//

  infinit::Identity
  _identity()
  {
    // XXX to improve so as not to use Infinit::User or Infinit::authority()

    elle::String path(common::infinit::identity_path(Infinit::User));

    infinit::Identity identity(elle::serialize::from_file(path));

    if (identity.validate(common::meta::certificate().subject_K()) == false)
      throw infinit::Exception(
        elle::sprintf("the identity '%s' is invalid", identity));

    return (identity);
  }

  infinit::Identity const&
  Agent::identity()
  {
    static infinit::Identity identity = _identity();

    return (identity);
  }

  cryptography::KeyPair
  _keypair()
  {
    // XXX to improve so as not to use the Agent::identity_passphrase.

    infinit::Identity const& identity = Agent::identity();

    cryptography::KeyPair keypair =
      identity.decrypt(Agent::identity_passphrase);

    return (keypair);
  }

  cryptography::KeyPair const&
  Agent::keypair()
  {
    static cryptography::KeyPair keypair = _keypair();

    return (keypair);
  }

  nucleus::neutron::Subject const&
  Agent::subject()
  {
    static nucleus::neutron::Subject subject(Agent::keypair().K());

    return (subject);
  }

  ///
  /// this method initializes the agent.
  ///
  elle::Status          Agent::Initialize()
  {
    // Load the tokpass which contains both the meta token
    // and the identity passphrase.
    {
      boost::filesystem::path identity_path(
        common::infinit::identity_path(Infinit::User));

      if (boost::filesystem::exists(identity_path) == false)
        throw infinit::Exception(
          elle::sprintf("the identity does to seem to exist: '%s'",
                        identity_path.string()));

      boost::filesystem::path tokpass_path(
        common::infinit::tokpass_path(Infinit::User));

      if (boost::filesystem::exists(tokpass_path) == true)
      {
        std::ifstream tokpass_content(tokpass_path.string());
        if (tokpass_content.good())
        {
          std::getline(tokpass_content, Agent::meta_token);
          std::getline(tokpass_content, Agent::identity_passphrase);
        }
      }
      else
      {
        // XXX temporary: we assume the passphrase to be empty!
        Agent::identity_passphrase = "";

        // prompt the user for the passphrase.
        /* XXX[to change to a better version where we retrieve the passphrase
         * from the watchdog]
         elle::String        prompt;

         prompt = "Enter passphrase for keypair '" + Infinit::User + "': ";

         if (elle::io::Console::Input(
         pass,
         prompt,
         elle::io::Console::OptionPassword) == elle::Status::Error)
         throw elle::Exception("unable to read the input");
        */
        // XXX[temporary fix]
      }
    }

    return elle::Status::Ok;
  }

  ///
  /// this method cleans the agent.
  ///
  /// the components are recycled just to make sure the memory is
  /// released before the Meta allocator terminates.
  ///
  elle::Status          Agent::Clean()
  {
    // nothing to do.

    return elle::Status::Ok;
  }

}
