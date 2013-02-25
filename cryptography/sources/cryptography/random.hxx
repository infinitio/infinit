#ifndef INFINIT_CRYPTOGRAPHY_RANDOM_HXX
# define INFINIT_CRYPTOGRAPHY_RANDOM_HXX

# include <cryptography/cryptography.hh>
# include <cryptography/Exception.hh>

# include <elle/Exception.hh>
# include <elle/Buffer.hh>
# include <elle/log.hh>
# include <elle/finally.hh>

# include <openssl/engine.h>
# include <openssl/rand.h>
# include <openssl/err.h>

/*----------------.
| Macro-functions |
`----------------*/

# define INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(_type_)                  \
  template <>                                                           \
  struct Generator<_type_>                                              \
  {                                                                     \
    static                                                              \
    _type_                                                              \
    generate()                                                          \
    {                                                                   \
      ELLE_LOG_COMPONENT("infinit.cryptography.random");                \
      ELLE_DEBUG_FUNCTION("");                                          \
                                                                        \
      return (_generate<_type_>());                                     \
    }                                                                   \
                                                                        \
    static                                                              \
    _type_                                                              \
    generate(_type_ minimum,                                            \
             _type_ maximum)                                            \
    {                                                                   \
      ELLE_LOG_COMPONENT("infinit.cryptography.random");                \
      ELLE_DEBUG_FUNCTION(minimum, maximum);                            \
                                                                        \
      return (_rangify(_generate<_type_>(), minimum, maximum));         \
    }                                                                   \
  };

namespace infinit
{
  namespace cryptography
  {
    namespace random
    {
      /*----------.
      | Functions |
      `----------*/

      template <typename T>
      T
      _rangify(T const value,
               T const minimum,
               T const maximum)
      {
        ELLE_LOG_COMPONENT("infinit.cryptography.random");
        ELLE_DEBUG_FUNCTION(value, minimum, maximum);

        ELLE_ASSERT(minimum <= maximum);

        T ranged =
          static_cast<T>(
            static_cast<elle::Real>(std::abs(value)) /
            (static_cast<elle::Real>(std::numeric_limits<T>::max()) -
             static_cast<elle::Real>(std::numeric_limits<T>::min())) *
            (static_cast<elle::Real>(maximum) -
             static_cast<elle::Real>(minimum)));

        return (minimum + ranged);
      }

      template <typename T>
      T
      _generate()
      {
        ELLE_LOG_COMPONENT("infinit.cryptography.random");
        ELLE_DEBUG_FUNCTION("");

        // Make sure the cryptographic system is set up.
        cryptography::require();

        T value;

        if (::RAND_bytes(reinterpret_cast<unsigned char*>(&value),
                         sizeof (value)) == 0)
          throw Exception(elle::sprintf("unable to generate random bytes: %s",
                                        ::ERR_error_string(ERR_get_error(),
                                                           nullptr)));

        return (value);
      }

      /*-----------.
      | Generators |
      `-----------*/

      template <typename T>
      struct Generator
      {
        static
        T
        generate()
        {
          static_assert(sizeof (T) == 0,
                        "unsupported type for random generation");
        }
      };

      template <>
      struct Generator<elle::Boolean>
      {
        static
        elle::Boolean
        generate()
        {
          ELLE_LOG_COMPONENT("infinit.cryptography.random");
          ELLE_DEBUG_FUNCTION("");

          elle::Integer32 value = _generate<elle::Boolean>();

          if (value > 0)
            return (true);
          else
            return (false);

          elle::unreachable();
        }
      };

      template <>
      struct Generator<elle::Real>
      {
        static
        elle::Real
        generate()
        {
          ELLE_LOG_COMPONENT("infinit.cryptography.random");
          ELLE_DEBUG_FUNCTION("");

          return (_generate<elle::Real>());
        }
      };

      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Character);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Integer8);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Integer16);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Integer32);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Integer64);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Natural8);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Natural16);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Natural32);
      INFINIT_CRYPTOGRAPHY_RANDOM_GENERATOR(elle::Natural64);

      template <>
      struct Generator<elle::String>
      {
        static
        elle::String
        generate(elle::Natural32 const length)
        {
          ELLE_LOG_COMPONENT("infinit.cryptography.random");
          ELLE_DEBUG_FUNCTION(length);

          // Make sure the cryptographic system is set up.
          cryptography::require();

          static elle::String const alphabet =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789@#$%^&*)_+=-\';:|.,<>?`~";

          elle::String value;

          value.resize(length);

          for (elle::Natural32 i = 0; i < length; i++)
          {
            elle::Natural32 n;

            if (::RAND_bytes(reinterpret_cast<unsigned char*>(&n),
                             sizeof (n)) == 0)
              throw Exception
                (elle::sprintf("unable to generate random bytes: %s",
                               ::ERR_error_string(ERR_get_error(), nullptr)));

            value[i] = alphabet[n % alphabet.length()];
          }

          return (value);
        }
      };

      template <>
      struct Generator<elle::Buffer>
      {
        static
        elle::Buffer
        generate(elle::Natural32 const size)
        {
          ELLE_LOG_COMPONENT("infinit.cryptography.random");
          ELLE_DEBUG_FUNCTION(size);

          // Make sure the cryptographic system is set up.
          cryptography::require();

          elle::Buffer value(size);

          // generate a random integer.
          if (::RAND_bytes(
                reinterpret_cast<unsigned char*>(value.mutable_contents()),
                value.size()) == 0)
            throw Exception(elle::sprintf("unable to generate random bytes: %s",
                                          ::ERR_error_string(ERR_get_error(),
                                                             nullptr)));

          return (value);
        }
      };

      /*----------.
      | Functions |
      `----------*/

      template <typename T,
                typename... A>
      T
      generate(A... arguments)
      {
        ELLE_LOG_COMPONENT("infinit.cryptography.random");
        ELLE_TRACE_FUNCTION("");

        return (Generator<T>::generate(arguments...));
      }
    }
  }
}

#endif
