#include <Program.hh>
#include <elle/io/Console.hh>
#include <elle/io/Unique.hh>
#include <elle/utility/Parser.hh>
#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/PublicKey.hh>
#include <cryptography/KeyPair.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <satellites/satellite.hh>
#include <satellites/authority/Authority.hh>

// XXX
#include <cryptography/all.hh>

namespace satellite
{

//
// ---------- definitions -----------------------------------------------------
//

  ///
  /// this value defines the authority key pair length.
  ///
  /// the length is kept high in order to make attacks more difficult.
  ///
  const elle::Natural32         Authority::Length = 4096;

  /// Create a new authority.
  elle::Status
  Authority::Create()
  {
    // Prompt the user for the passphrase.
    elle::String prompt = "Enter passphrase for the authority keypair: ";
    elle::String pass;
    if (elle::io::Console::Input(
          pass,
          prompt,
          elle::io::Console::OptionPassword) == elle::Status::Error)
      throw elle::Exception("unable to read the input");

    // Create the authority with the generated key pair.
    elle::Authority authority{
      cryptography::KeyPair::generate(
        cryptography::Cryptosystem::rsa,
        Authority::Length)};

    // Encrypt the authority.
    if (authority.Encrypt(pass) == elle::Status::Error)
      throw elle::Exception("unable to encrypt the authority");

    // Store the authority.
    authority.store(elle::io::Path(lune::Lune::Authority));

    return elle::Status::Ok;
  }

  /// Destroy the existing authority.
  elle::Status
  Authority::Destroy()
  {
    // Erase the authority file.
    elle::Authority::erase(elle::io::Path(lune::Lune::Authority));

    return elle::Status::Ok;
  }

  ///
  /// this method retrieves and displays information on the authority.
  ///
  elle::Status          Authority::Information()
  {
    elle::String        prompt;
    elle::String        pass;
    elle::io::Unique        unique;

    // check if the authority exists.
    if (elle::Authority::exists(elle::io::Path(lune::Lune::Authority)) == false)
      throw elle::Exception("unable to locate the authority file");

    // prompt the user for the passphrase.
    prompt = "Enter passphrase for the authority keypair: ";

    if (elle::io::Console::Input(
          pass,
          prompt,
          elle::io::Console::OptionPassword) == elle::Status::Error)
      throw elle::Exception("unable to read the input");

    // Load the authority.
    elle::Authority authority{elle::io::Path(lune::Lune::Authority)};

    // decrypt the authority.
    if (authority.Decrypt(pass) == elle::Status::Error)
      throw elle::Exception("unable to decrypt the authority");

    // dump the authority.
    if (authority.Dump() == elle::Status::Error)
      throw elle::Exception("unable to dump the authority");

    // retrive the public key's unique.
    if (authority.K().Save(unique) == elle::Status::Error)
      throw elle::Exception("unable to save the authority's public key");

    // dump the public key's unique so that it can be easily hard-coded in the
    // infinit software sources.
    std::cout << "[Unique] " << unique << std::endl;

    return elle::Status::Ok;
  }

//
// ---------- functions -------------------------------------------------------
//

  ///
  /// the main function.
  ///
  static
  elle::Status
  Authority(elle::Natural32 argc,
            elle::Character* argv[])
  {
    Authority::Operation        operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Authority") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the operation.
    operation = Authority::OperationUnknown;

    // allocate a new parser.
    Infinit::Parser = new elle::utility::Parser(argc, argv);

    // specify a program description.
    if (Infinit::Parser->Description(Infinit::Copyright) == elle::Status::Error)
      throw elle::Exception("unable to set the description");

    // register the options.
    if (Infinit::Parser->Register(
          "Help",
          'h',
          "help",
          "display the help",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Create",
          'c',
          "create",
          "create the authority",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Destroy",
          'd',
          "destroy",
          "destroy the existing authority",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Information",
          'x',
          "information",
          "display information regarding the authority",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // parse.
    if (Infinit::Parser->Parse() == elle::Status::Error)
      throw elle::Exception("unable to parse the command line");

    // test the option.
    if (Infinit::Parser->Test("Help") == true)
      {
        // display the usage.
        Infinit::Parser->Usage();

        // quit.
        return elle::Status::Ok;
      }

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("Create") == true) &&
        (Infinit::Parser->Test("Destroy") == true) &&
        (Infinit::Parser->Test("Information") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("the create, destroy and information options are "
               "mutually exclusive");
      }

    // test the option.
    if (Infinit::Parser->Test("Create") == true)
      operation = Authority::OperationCreate;

    // test the option.
    if (Infinit::Parser->Test("Destroy") == true)
      operation = Authority::OperationDestroy;

    // test the option.
    if (Infinit::Parser->Test("Information") == true)
      operation = Authority::OperationInformation;

    // trigger the operation.
    switch (operation)
      {
      case Authority::OperationCreate:
        {
          // create the authority.
          if (Authority::Create() == elle::Status::Error)
            throw elle::Exception("unable to create the authority");

          // display a message.
          std::cout << "The authority has been created successfully!"
                    << std::endl;

          break;
        }
      case Authority::OperationDestroy:
        {
          // destroy the authority.
          if (Authority::Destroy() == elle::Status::Error)
            throw elle::Exception("unable to destroy the authority");

          // display a message.
          std::cout << "The authority has been destroyed successfully!"
                    << std::endl;

          break;
        }
      case Authority::OperationInformation:
        {
          // get information on the authority.
          if (Authority::Information() == elle::Status::Error)
            throw elle::Exception("unable to retrieve information on the authority");

          break;
        }
      case Authority::OperationUnknown:
      default:
        {
          // display the usage.
          Infinit::Parser->Usage();

          throw elle::Exception("please specify an operation to perform");
        }
      }

    // delete the parser.
    delete Infinit::Parser;
    Infinit::Parser = nullptr;

    // clean Infinit.
    if (Infinit::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Infinit");

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");

    return elle::Status::Ok;
  }

}

int main(int argc, char** argv)
{
  return satellite_main("8authority", std::bind(satellite::Authority,
                                                argc, argv));
}
