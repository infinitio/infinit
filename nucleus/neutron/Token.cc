#include <nucleus/neutron/Token.hh>

#include <cryptography/SecretKey.hh>
#include <cryptography/Code.hh>
#include <cryptography/PublicKey.hh>
#include <cryptography/PrivateKey.hh>

namespace nucleus
{
  namespace neutron
  {

    /*---------------.
    | Static Methods |
    `---------------*/

    Token const&
    Token::null()
    {
      static Token token(Token::Type::null);

      return (token);
    }

    /*-------------.
    | Construction |
    `-------------*/

    Token::Token():
      _valid(nullptr)
    {
    }

    Token::Token(const Token& other):
      _type(other._type),
      _valid(nullptr)
    {
      if (other._valid != nullptr)
        this->_valid = new Valid(other._valid->code());
    }

    Token::Token(Type const type):
      _type(type),
      _valid(nullptr)
    {
      switch (this->_type)
        {
        case Type::null:
          {
            // Nothing to do; this is the right way to construct such special
            // tokens.
            break;
          }
        case Type::valid:
          {
            throw Exception("valid tokens cannot be built through this "
                            "constructor");
          }
        default:
          throw Exception(elle::sprintf("unknown token type '%s'",
                                        this->_type));
        }
    }

    Token::~Token()
    {
      delete this->_valid;
    }

    Token::Valid::Valid()
    {
    }

    Token::Valid::Valid(cryptography::Code const& code):
      _code(code)
    {
    }

    /*----------.
    | Operators |
    `----------*/

    elle::Boolean
    Token::operator ==(Token const& other) const
    {
      if (this == &other)
        return true;

      if (this->_type != other._type)
        return (false);

      if (this->_type == Type::valid)
        {
          ELLE_ASSERT(this->_valid != nullptr);
          ELLE_ASSERT(other._valid != nullptr);

          if ((this->_valid->code() != other._valid->code()))
            return (false);
        }

      return (true);
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status        Token::Dump(elle::Natural32     margin) const
    {
      elle::String      alignment(margin, ' ');

      switch (this->_type)
        {
        case Type::null:
          {
            std::cout << alignment << "[Token] " << "none" << std::endl;

            break;
          }
        case Type::valid:
          {
            ELLE_ASSERT(this->_valid != nullptr);

            std::cout << alignment << "[Token] "
                      << this->_valid->code() << std::endl;

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown token type '%s'",
                                        this->_type));
        }

      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Token::print(std::ostream& stream) const
    {
      switch (this->_type)
        {
        case Type::null:
          {
            stream << "token(null)";
            break;
          }
        case Type::valid:
          {
            ELLE_ASSERT(this->_valid != nullptr);

            stream << "code("
                   << this->_valid->code()
                   << ")";

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown token type '%s'",
                                        this->_type));
        }
    }

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Token::Type const type)
    {
      switch (type)
        {
        case Token::Type::null:
          {
            stream << "null";
            break;
          }
        case Token::Type::valid:
          {
            stream << "valid";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown token type: '%s'",
                                          static_cast<int>(type)));
          }
        }

      return (stream);
    }
  }
}
