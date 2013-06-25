#include <elle/serialize/extract.hh>

#include <satellites/satellite.hh>

#include <infinit/all.hh>

#include <lune/Lune.hh>

#include <chrono>

namespace satellite
{
  void
  Certificate(elle::Natural32 argc,
              elle::Character* argv[])
  {
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // Example for Antony: infinit-certificate-create
    {

      /* Examples of authorities
        infinit: the absolute origin authority
        infinit.meta.repository: the meta authority for signing identities etc.
        infinit.meta.authentication: the meta authority for challenging
      */

      elle::String issuer_path("XXX");
      elle::String issuer_passphrase("XXX");

      elle::String subject_path("XXX");

      elle::String certificate_path("XXX");

      elle::String description("XXX");

      infinit::certificate::Permissions permissions =
        infinit::certificate::permissions::none; // XXX

      // Two years before current time.
      std::chrono::minutes minutes_in_the_past(2 * 365 * 24 * 60);
      // Three years after current time.
      std::chrono::minutes minutes_in_the_future(3 * 365 * 24 * 60);

      // ---

      infinit::Authority issuer(elle::serialize::from_file(issuer_path));
      // XXX validate
      cryptography::PrivateKey issuer_k = issuer.decrypt(issuer_passphrase);

      infinit::Authority subject(elle::serialize::from_file(subject_path));
      // XXX validate

      auto now = std::chrono::system_clock::now();

      std::chrono::system_clock::time_point valid_from =
        now + minutes_in_the_past;
      std::chrono::system_clock::time_point valid_until =
        now + minutes_in_the_future;

      infinit::Certificate certificate(issuer.K(),
                                       subject.K(),
                                       description,
                                       permissions,
                                       valid_from,
                                       valid_until,
                                       issuer_k);

      elle::serialize::to_file(certificate_path) << certificate;

      elle::String archive;
      elle::serialize::to_string<
        elle::serialize::OutputBase64Archive>(archive) << certificate;
      elle::printf("%s\n", archive);
    }

    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");
  }
}

int                     main(int                                argc,
                             char**                             argv)
{
  return satellite_main("8certificate", [&] {
                          satellite::Certificate(argc, argv);
                        });
}
