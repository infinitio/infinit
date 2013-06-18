#include <infinit/infinit.hh>

#include <elle/serialize/extract.hh>
#include <elle/serialize/Base64Archive.hh>

namespace infinit
{
  namespace certificate
  {
    /*----------.
    | Functions |
    `----------*/

    Certificate const&
    origin()
    {
      static Certificate certificate(
        elle::serialize::from_string<
        elle::serialize::InputBase64Archive>("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"));

      return (certificate);
    }
  }
}
