#include <satellites/dump/Dump.hh>
#include <satellites/satellite.hh>

#include <elle/utility/Parser.hh>
#include <elle/serialize/extract.hh>
#include <elle/log.hh>

#include <nucleus/proton/Block.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/factory.hh>

#include <common/common.hh>

#include <Infinit.hh>
#include <Program.hh>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.satellites.dump.Dump");

namespace satellite
{
  void
  Dump(elle::Natural32 argc,
       elle::Character* argv[])
  {
    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Dump") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

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

    // register the option.
    if (Infinit::Parser->Register(
          "Path",
          'p',
          "path",
          "specifies the path to the block to dump",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the option.
    if (Infinit::Parser->Register(
          "Component",
          'c',
          "component",
          "specifies the component number of the block to dump",
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

        // quit.
        return;
      }

    elle::String path;

    // retrieve the path.
    if (Infinit::Parser->Value("Path",
                               path) == elle::Status::Error)
      throw elle::Exception("unable to retrieve the path value");

    ELLE_DEBUG("path '%s'", path);

    elle::String value;
    nucleus::neutron::Component component;

    // retrieve the path.
    if (Infinit::Parser->Value("Component",
                               value) == elle::Status::Error)
      throw elle::Exception("unable to retrieve the component value");

    component =
      static_cast<nucleus::neutron::Component>(
        boost::lexical_cast<elle::Natural32>(value));

    ELLE_DEBUG("component '%s'", component);

    // Create an empty block.
    auto const& factory = nucleus::factory::block<elle::serialize::NoInit>();

    std::unique_ptr<nucleus::proton::Block> block(
      factory.allocate<nucleus::proton::Block>(component,
                                               elle::serialize::no_init));

    // Deserialize the block.
    elle::serialize::from_file(path) >> *block;

    // Dump the block content.
    block->Dump();

    // Bind the block
    nucleus::proton::Address address = block->bind();

    // Validate the block.
    block->validate(address);

    // Compare the bound address with the one given on the command line.
    boost::filesystem::path pathed{path};
    boost::filesystem::path stemed{pathed.stem()};

    elle::printf("comparing the given address '%s' (based on the path) "
                 "with the bound one '%s'\n",
                 stemed.string(), address.unique());

    if (stemed.string() != address.unique())
      throw elle::Exception(
        elle::sprintf("the bound address '%s' does not match the given "
                      "one '%s'",
                      address.unique(),
                      stemed.string()));

    // delete the parser.
    delete Infinit::Parser;
    Infinit::Parser = nullptr;

    // clean Infinit.
    if (Infinit::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Infinit");
  }
}

int
main(int argc,
     char** argv)
{
  return satellite_main("8dump",
                        [&] {
                          satellite::Dump(argc, argv);
                        });
}
