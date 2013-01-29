#include <cryptography/oneway.hh>
#include <cryptography/cryptography.hh>
#include <cryptography/finally.hh>

#include <elle/log.hh>

#include <openssl/evp.h>
#include <openssl/err.h>

ELLE_LOG_COMPONENT("infinit.cryptograhy.oneway");

/*----------------.
| Macro-functions |
`----------------*/

namespace infinit
{
  namespace cryptography
  {
    namespace oneway
    {
      /*-----------------.
      | Static Functions |
      `-----------------*/

      /// Resolve a algorithm name into an EVP function pointer.
      static
      ::EVP_MD const*
      resolve(Algorithm const name)
      {
        switch (name)
          {
          case Algorithm::md5:
            return (::EVP_md5());
          case Algorithm::sha:
            return (::EVP_sha());
          case Algorithm::sha1:
            return (::EVP_sha1());
          case Algorithm::sha224:
            return (::EVP_sha224());
          case Algorithm::sha256:
            return (::EVP_sha256());
          case Algorithm::sha384:
            return (::EVP_sha384());
          case Algorithm::sha512:
            return (::EVP_sha512());
          default:
            throw elle::Exception("unable to resolve the given one-way "
                                  "function name");
          }

        elle::unreachable();
      }

      /*---------------.
      | Static Methods |
      `---------------*/

      Digest
      hash(Plain const& plain,
           Algorithm algorithm)
      {
        ELLE_TRACE_FUNCTION(plain, algorithm);

        ::EVP_MD const* function = resolve(algorithm);
        Digest digest(EVP_MD_size(function));

        // Initialise the context.
        ::EVP_MD_CTX context;

        ::EVP_MD_CTX_init(&context);

        INFINIT_CRYPTOGRAPHY_FINALLY_ACTION_CLEANUP_DIGEST_CONTEXT(context);

        // Initialise the digest.
        if (::EVP_DigestInit_ex(&context, function, nullptr) <= 0)
          throw elle::Exception("%s",
                                ::ERR_error_string(ERR_get_error(), nullptr));

        ELLE_ASSERT(plain.buffer().contents() != nullptr);

        // Update the digest with the given plain's data.
        if (::EVP_DigestUpdate(&context,
                               plain.buffer().contents(),
                               plain.buffer().size()) <= 0)
          throw elle::Exception("%s",
                                ::ERR_error_string(ERR_get_error(), nullptr));

        // Finalise the digest.
        unsigned int size;

        if (::EVP_DigestFinal_ex(&context,
                                 digest.buffer().mutable_contents(),
                                 &size) <=0)
          throw elle::Exception("%s",
                                ::ERR_error_string(ERR_get_error(), nullptr));

        // Update the digest final size.
        digest.buffer().size(size);

        // Clean the context.
        if (::EVP_MD_CTX_cleanup(&context) <= 0)
          throw elle::Exception("unable to clean the context: %s",
                                ::ERR_error_string(ERR_get_error(), nullptr));

        INFINIT_CRYPTOGRAPHY_FINALLY_ABORT(context);

        return (digest);
      }
    }
  }
}
