#include <boost/uuid/string_generator.hpp>

#include <elle/log.hh>

#include <reactor/network/exception.hh>

#include <infinit/oracles/trophonius/server/Meta.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>
#include <infinit/oracles/trophonius/server/User.hh>
#include <infinit/oracles/trophonius/server/exceptions.hh>
#include <infinit/oracles/trophonius/server/utils.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.Meta")

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        Meta::Meta(Trophonius& trophonius,
                   std::unique_ptr<reactor::network::TCPSocket>&& socket):
          Client(trophonius, std::move(socket))
        {
          ELLE_DEBUG("%s: connected", *this);
          // XXX: Ensure uncity.
        }

        Meta::~Meta()
        {
          ELLE_DEBUG("%s: disconnected", *this);
        }

        void
        Meta::_handle()
        {
          try
          {
            auto json = read_json(*this->_socket);

            static auto mandatory_fields = {"notification_type", "device_id"};

            for (auto const& field: mandatory_fields)
              if (!json.isMember(field))
                throw ProtocolError(
                  elle::sprintf("mandatory field %s missing", field));

            User& user = this->trophonius().user(
              boost::uuids::string_generator()(json["device_id"].asString()));
            user.notify(json);
          }
          catch (ProtocolError const& e)
          {
            ELLE_WARN("%s: protocol error: %s", *this, e.what());
          }
          catch (UnknownClient const& e)
          {
            ELLE_WARN("%s: unknown user: %s", *this, e.what());
          }
          catch (AuthenticationError const& e)
          {
            ELLE_WARN("%s: authentication error: %s", *this, e.what());
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: network error: %s", *this, e.what());
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          catch (...)
          {
            ELLE_WARN("%s: unknown error: %s", *this, elle::exception_string());
          }
        }

        /*----------.
        | Printable |
        `----------*/
        void
        Meta::print(std::ostream& stream) const
        {
          stream << "Meta(" << this->_socket->peer() << ")";
        }
      }
    }
  }
}
