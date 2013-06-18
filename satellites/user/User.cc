#include <elle/io/Console.hh>
#include <elle/io/Directory.hh>
#include <elle/io/Piece.hh>
#include <elle/utility/Parser.hh>
#include <elle/serialize/insert.hh>

#include <cryptography/PublicKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <infinit/Certificate.hh>

#include <common/common.hh>

#include <etoile/Etoile.hh>

#include <lune/Dictionary.hh>
#include <lune/Lune.hh>

#include <infinit/Identity.hh>
#include <infinit/Authority.hh>

#include <satellites/satellite.hh>
#include <satellites/user/User.hh>

#include <Infinit.hh>
#include <Program.hh>

#include <boost/filesystem.hpp>

namespace satellite
{
  ///
  /// this method creates a new user by generating a new key pair and
  /// storing a user block.
  ///
  elle::Status          User::Create(elle::String const&        identifier,
                                     const elle::String&        name,
                                     elle::String const& authority_path)
  {
    elle::String        prompt;
    elle::String        pass;
    lune::Dictionary    dictionary;

    boost::filesystem::path path(
      common::infinit::identity_path(identifier));

    // check the argument.
    if (name.empty() == true)
      throw elle::Exception("unable to create a user without a user name");

    // check if the user already exists.
    if (boost::filesystem::exists(path) == true)
      throw elle::Exception("this user seems to already exist");

    // prompt the user for the passphrase.
    prompt = "Enter passphrase for the authority: ";

    if (elle::io::Console::Input(
          pass,
          prompt,
          elle::io::Console::OptionPassword) == elle::Status::Error)
      throw elle::Exception("unable to read the input");

    // load the authority.
    infinit::Authority authority(elle::serialize::from_file(authority_path));

    cryptography::PrivateKey authority_k = authority.decrypt(pass);

    // prompt the user for the passphrase.
    prompt = "Enter passphrase for keypair '" + name + "': ";

    if (elle::io::Console::Input(
          pass,
          prompt,
          elle::io::Console::OptionPassword) == elle::Status::Error)
      throw elle::Exception("unable to read the input");

    infinit::Identity identity(authority.K(),
                               identifier,
                               name,
                               cryptography::KeyPair::generate(
                                 cryptography::Cryptosystem::rsa,
                                 2048), // XXX make an option for that
                               pass,
                               authority_k);

    elle::serialize::to_file(path.string()) << identity;

    // store an empty dictionary.
    dictionary.store(name);

    return elle::Status::Ok;
  }

  ///
  /// this method destroys an existing user.
  ///
  elle::Status          User::Destroy(const elle::String&       name)
  {
    elle::String identifier(name);

    boost::filesystem::path path(
      common::infinit::identity_path(identifier));

    //
    // remove the identity.
    //
    {
      // check the argument.
      if (name.empty() == true)
        throw elle::Exception("unable to destroy a user without a user name");

      // check if the user already exists.
      if (boost::filesystem::exists(identifier) == false)
        throw elle::Exception("this user does not seem to exist");

      // destroy the identity.
      elle::io::File::Erase(elle::io::Path{path.string()});
    }

    //
    // remove the dictionary, if necessary.
    //
    {
      lune::Dictionary  dictionary;

      // if the dictionary exists...
      if (lune::Dictionary::exists(name) == true)
        lune::Dictionary::erase(name);
    }

    //
    // remove the user directory.
    //
    {
      elle::io::Path        path;

      // create the user path.
      if (path.Create(lune::Lune::User) == elle::Status::Error)
        throw elle::Exception("unable to create the path");

      // complete the path with the user name.
      if (path.Complete(elle::io::Piece("%USER%", name)) == elle::Status::Error)
        throw elle::Exception("unable to complete the path");

      // clear the user directory content.
      if (elle::io::Directory::Clear(path) == elle::Status::Error)
        throw elle::Exception("unable to clear the directory");

      // remove the directory.
      if (elle::io::Directory::Remove(path) == elle::Status::Error)
        throw elle::Exception("unable to erase the directory");
    }

    return elle::Status::Ok;
  }

  ///
  /// this method displays information on the given user.
  ///
  elle::Status          User::Information(const elle::String&   name)
  {
    elle::String        prompt;
    elle::String        pass;
    cryptography::PublicKey     K;

    elle::String identifier(name);

    boost::filesystem::path path(
      common::infinit::identity_path(identifier));

    // check the argument.
    if (name.empty() == true)
      throw elle::Exception("unable to create a user without a user name");

    // check if the user already exists.
    if (boost::filesystem::exists(path.string()) == false)
      throw elle::Exception("this user does not seem to exist");

    // prompt the user for the passphrase.
    prompt = "Enter passphrase for keypair '" + name + "': ";

    if (elle::io::Console::Input(
          pass,
          prompt,
          elle::io::Console::OptionPassword) == elle::Status::Error)
      throw elle::Exception("unable to read the input");

    infinit::Identity identity(
      elle::serialize::from_file(
        common::infinit::identity_path(identifier)));

    // verify the identity.
    if (identity.validate(common::meta::certificate().subject_K()) == false)
      throw elle::Exception("invalid identity");

    cryptography::KeyPair keypair = identity.decrypt(pass);

    // dump the identity.
    std::cout << identity << std::endl;

    elle::String unique;
    elle::serialize::to_string<elle::serialize::OutputBase64Archive>(unique) <<
      identity;

    std::cout << "[Unique] " << unique << std::endl;

    return elle::Status::Ok;
  }

  void
  User(elle::Natural32 argc, elle::Character* argv[])
  {
    User::Operation     operation;

    // set up the program.
    if (elle::concurrency::Program::Setup("User") == elle::Status::Error)
      throw std::runtime_error("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw std::runtime_error("unable to initialize Lune");

    // initialize the operation.
    operation = User::OperationUnknown;

    // allocate a new parser.
    Infinit::Parser = new elle::utility::Parser(argc, argv);

    // specify a program description.
    if (Infinit::Parser->Description(Infinit::Copyright) == elle::Status::Error)
      throw std::runtime_error("unable to set the description");

    // register the options.
    if (Infinit::Parser->Register(
          "Help",
          'h',
          "help",
          "display the help",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw std::runtime_error("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Create",
          'c',
          "create",
          "create a user",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw std::runtime_error("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Destroy",
          'd',
          "destroy",
          "destroy an existing network",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw std::runtime_error("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Information",
          'x',
          "information",
          "display information regarding the user",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw std::runtime_error("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Identifier",
          'i',
          "identifier",
          "specify the identity",
          elle::utility::Parser::KindOptional) == elle::Status::Error)
      throw std::runtime_error("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Name",
          'n',
          "name",
          "specify the user name",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw std::runtime_error("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Authority",
          'A',
          "authority",
          "specify the path to the authority file",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // parse.
    if (Infinit::Parser->Parse() == elle::Status::Error)
      throw std::runtime_error("unable to parse the command line");

    // test the option.
    if (Infinit::Parser->Test("Help") == true)
      {
        // display the usage.
        Infinit::Parser->Usage();
        return;
      }

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("Create") == true) &&
        (Infinit::Parser->Test("Destroy") == true) &&
        (Infinit::Parser->Test("Information") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw std::runtime_error("the create, destroy and information options are "
               "mutually exclusive");
      }

    // test the option.
    if (Infinit::Parser->Test("Create") == true)
      operation = User::OperationCreate;

    // test the option.
    if (Infinit::Parser->Test("Destroy") == true)
      operation = User::OperationDestroy;

    // test the option.
    if (Infinit::Parser->Test("Information") == true)
      operation = User::OperationInformation;

    // trigger the operation.
    switch (operation)
      {
      case User::OperationCreate:
        {
          elle::String          identifier;
          elle::String          name;

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw std::runtime_error("unable to retrieve the name value");

          // retrieve the identifier.
          if (Infinit::Parser->Value("Identifier", identifier, name) == elle::Status::Error)
            throw std::runtime_error("unable to retrieve the identifier");

          elle::String authority_path;
          if (Infinit::Parser->Value("Authority",
                                     authority_path) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the model value");

          // create a user.
          if (User::Create(identifier, name, authority_path) == elle::Status::Error)
            throw std::runtime_error("unable to create the user");

          // display a message.
          std::cout << "The user has been created successfully!"
                    << std::endl;

          break;
        }
      case User::OperationDestroy:
        {
          elle::String          name;

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw std::runtime_error("unable to retrieve the name value");

          // destroy a user.
          if (User::Destroy(name) == elle::Status::Error)
            throw std::runtime_error("unable to destroy the user");

          // display a message.
          std::cout << "The user has been destroyed successfully!"
                    << std::endl;

          break;
        }
      case User::OperationInformation:
        {
          elle::String          name;

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw std::runtime_error("unable to retrieve the name value");

          // display information.
          if (User::Information(name) == elle::Status::Error)
            throw std::runtime_error("unable to display information on the user");

          break;
        }
      case User::OperationUnknown:
      default:
        {
          // display the usage.
          Infinit::Parser->Usage();

          throw std::runtime_error("please specify an operation to perform");
        }
      }

    // delete the parser.
    delete Infinit::Parser;
    Infinit::Parser = nullptr;

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw std::runtime_error("unable to clean Lune");
  }
}

int
main(int argc, char** argv)
{
  return satellite_main("8user", std::bind(satellite::User, argc, argv));
}
