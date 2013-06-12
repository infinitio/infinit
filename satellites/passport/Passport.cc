#include <satellites/passport/Passport.hh>
#include <satellites/satellite.hh>

#include <Infinit.hh>

#include <infinit/Identity.hh>

#include <elle/io/Console.hh>
#include <elle/io/Path.hh>
#include <elle/io/Piece.hh>
#include <elle/utility/Parser.hh>

#include <cryptography/random.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/Authority.hh>
#include <hole/Hole.hh>
#include <hole/Passport.hh>

#include <lune/Lune.hh>

#include <common/common.hh>

#include <Program.hh>

namespace satellite
{
//
// ---------- methods ---------------------------------------------------------
//

  ///
  /// this method creates a new passport.
  ///
  void
  Passport::Create(elle::String const& user,
                   elle::String const& passport_name)
  {
    //
    // test the arguments.
    //
    {
      // check if the authority exists.
      if (elle::Authority::exists(elle::io::Path(lune::Lune::Authority))
          == false)
        throw elle::Exception("unable to locate the authority file");
    }

    // Retrieve the authority.
    elle::String              prompt;
    elle::String              pass;

    // prompt the user for the passphrase.
    prompt = "Enter passphrase for the authority: ";

    if (elle::io::Console::Input(
          pass,
          prompt,
          elle::io::Console::OptionPassword) == elle::Status::Error)
      throw elle::Exception("unable to read the input");

    // load the authority.
    elle::Authority authority(elle::io::Path{lune::Lune::Authority});

    // decrypt the authority.
    if (authority.Decrypt(pass) == elle::Status::Error)
      throw elle::Exception("unable to decrypt the authority");

    //
    // create the passport.
    //
    {
      elle::Natural32 const id_length = 128;

      elle::String id{
        cryptography::random::generate<elle::String>(id_length)
      };

      infinit::Identity identity(
        elle::serialize::from_file(common::infinit::identity_path(user)));

      cryptography::KeyPair keypair = identity.decrypt(pass);

      elle::Passport passport{
        id, passport_name, keypair.K(), authority
      };

      elle::io::Path passport_path(lune::Lune::Passport);
      passport_path.Complete(elle::io::Piece{"%USER%", user});

      // store the passport.
      passport.store(passport_path);
    }
  }

  ///
  /// this method destroys the existing passport.
  ///
  elle::Status          Passport::Destroy(elle::String const& user)
  {
    elle::io::Path passport_path(lune::Lune::Passport);
    passport_path.Complete(elle::io::Piece{"%USER%", user});

    // does the passport exist.
    if (elle::Passport::exists(passport_path) == true)
      elle::Passport::erase(passport_path);

    return elle::Status::Ok;
  }

  ///
  /// this method retrieves and displays information on the passport.
  ///
  elle::Status          Passport::Information(elle::String const& user)
  {
    elle::io::Path passport_path(lune::Lune::Passport);
    passport_path.Complete(elle::io::Piece{"%USER%", user});

    //
    // test the arguments.
    //
    {
      // does the passport exist.
      if (elle::Passport::exists(passport_path) == false)
        throw elle::Exception("this passport does not seem to exist");
    }

    elle::Passport      passport;

    //
    // retrieve the passport.
    //
    {
      // load the passport.
      passport.load(passport_path);

      // validate the passport.
      if (passport.validate(Infinit::authority()) == false)
        throw elle::Exception("unable to validate the passport");
    }

    passport.dump();

    return elle::Status::Ok;
  }

//
// ---------- functions -------------------------------------------------------
//

  void
  Passport(elle::Natural32 argc,
           elle::Character* argv[])
  {
    Passport::Operation operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Passport") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the operation.
    operation = Passport::OperationUnknown;

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
          "create a new passport",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Destroy",
          'd',
          "destroy",
          "destroy an existing passport",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Information",
          'x',
          "information",
          "display information regarding a passport",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the option.
    if (Infinit::Parser->Register(
          "User",
          'u',
          "user",
          "specifies the name of the user",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    if (Infinit::Parser->Register(
          "Name",
          'n',
          "name",
          "specifies the name of the passport (device)",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // parse.
    if (Infinit::Parser->Parse() == elle::Status::Error)
      throw elle::Exception("unable to parse the command line");

    // test the option.
    if (Infinit::Parser->Test("Help") == true)
      {
        // display the usage.
        Infinit::Parser->Usage();
        return;
      }

    // retrieve the user name.
    if (Infinit::Parser->Value("User",
                               Infinit::User) == elle::Status::Error)
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("unable to retrieve the user name");
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
      operation = Passport::OperationCreate;

    // test the option.
    if (Infinit::Parser->Test("Destroy") == true)
      operation = Passport::OperationDestroy;

    // test the option.
    if (Infinit::Parser->Test("Information") == true)
      operation = Passport::OperationInformation;

    // trigger the operation.
    switch (operation)
      {
      case Passport::OperationCreate:
        {
          std::string passport_name;
          if (Infinit::Parser->Value("Name",
                                     passport_name) == elle::Status::Error)
            {
              // display the usage.
              Infinit::Parser->Usage();

              throw elle::Exception("unable to retrieve the passport name");
            }

          // create the passport.
          Passport::Create(Infinit::User, passport_name);

          // display a message.
          std::cout << "The passport has been created successfully!"
                    << std::endl;

          break;
        }
      case Passport::OperationDestroy:
        {
          // destroy the passport.
          if (Passport::Destroy(Infinit::User) == elle::Status::Error)
            throw elle::Exception("unable to destroy the passport");

          // display a message.
          std::cout << "The passport has been destroyed successfully!"
                    << std::endl;

          break;
        }
      case Passport::OperationInformation:
        {
          // get information on the passport.
          if (Passport::Information(Infinit::User) == elle::Status::Error)
            throw elle::Exception("unable to retrieve information on the passport");

          break;
        }
      case Passport::OperationUnknown:
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
  }
}

//
// ---------- main ------------------------------------------------------------
//

int                     main(int                                argc,
                             char**                             argv)
{
  return satellite_main("8passport", [&] {
                          satellite::Passport(argc, argv);
                        });
}
