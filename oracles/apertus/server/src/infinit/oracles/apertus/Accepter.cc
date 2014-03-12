#include <infinit/oracles/apertus/Accepter.hh>
#include <infinit/oracles/apertus/Apertus.hh>

#include <reactor/network/socket.hh>
#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>

#include <elle/log.hh>
#include <elle/finally.hh>
#include <elle/utility/Move.hh>

ELLE_LOG_COMPONENT("infinit.oracles.apertus.Accepter");

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      Accepter::Accepter(Apertus& apertus,
                         std::unique_ptr<reactor::network::Socket>&& client,
                         reactor::Duration timeout):
        _apertus(apertus),
        _client(std::move(client)),
        _accepter(reactor::Thread::make_tracked(
          *reactor::Scheduler::scheduler(),
          elle::sprintf("accept-%s", this),
          [this] { this->_handle(); }
          )),
        _timeout(elle::sprintf("timeout-%s", this),
          timeout,
          [this]
          {
            ELLE_TRACE("%s: client timeout", *this);
            // Terminating accepter thread does not have the semantic we need
            _client->close();
          })
      {
        ELLE_TRACE_SCOPE("%s: construction", *this);
        ELLE_ASSERT(this->_client != nullptr);
      }

      Accepter::~Accepter()
      {
        ELLE_TRACE_SCOPE("%s: deletion:accepter", *this);
        this->_accepter->terminate_now(false);
        this->_timeout.terminate_now(false);
      }

      void
      Accepter::_handle()
      {
        /* Warning, 'this' survives termination of _handle() in one case.
        */
        ELLE_TRACE_SCOPE("%s: start handling client", *this);

        ELLE_ASSERT(this->_client != nullptr);

        try
        {
          // Retrieve TID size.
          unsigned char size;
          reactor::network::Buffer tmp(&size, 1);
          ELLE_TRACE("%s: reading for the size of the identifier", *this)
            this->_client->read(tmp);
          ELLE_DEBUG("%s: identifier size: %i", *this, size);

          // Retrieve TID of the client.
          char tid_array[size + 1];
          tid_array[size] = '\0';
          reactor::network::Buffer tid_buffer(tid_array, size);

          ELLE_TRACE("%s: reading for the identifier", *this)
            this->_client->read(tid_buffer);
          Apertus::TID tid = std::string(tid_array);
          ELLE_DEBUG("%s: identifier: %s", *this, tid);

          // First client to connect with this TID, it must wait.
          auto peer_iterator = this->_apertus._clients.find(tid);
          auto self = this->_apertus._take_from_accepters(this);
          if (peer_iterator == this->_apertus._clients.end())
          {
            ELLE_TRACE("%s: first user for TID %s", *this, tid);
            this->_apertus._clients[tid] = std::move(self);
          }
          // Second client, establishing connection.
          else
          {
            ELLE_TRACE("%s: peer found for TID %s", *this, tid)
            {
              // extract both sockets, and remove this from accepters, and peer from clients
              auto peer_acceptor = std::move(peer_iterator->second);
              auto peer_socket = std::move(peer_acceptor->_client);
              // stop timers
              peer_acceptor->_timeout.terminate_now();
              _timeout.terminate_now();
              this->_apertus._clients.erase(peer_iterator);
              ELLE_DEBUG("%s: connect users", *this);
              this->_apertus._connect(
                tid,
                std::move(this->_client),
                std::move(peer_socket));
              ELLE_DEBUG("%s: finish", *this);
            }
          }
        }
        catch (reactor::Terminate const&)
        {
          ELLE_TRACE("%s: eplicitly terminated", *this);
        }
        catch (reactor::network::ConnectionClosed const&)
        {
          ELLE_TRACE("%s: connection closed", *this);
        }
        catch (...)
        {
          ELLE_ERR("%s: swallowed exception: %s",
                   *this, elle::exception_string());
        }
      }

      /*----------.
      | Printable |
      `----------*/
      void
      Accepter::print(std::ostream& stream) const
      {
        stream << "Accepter " << this->_client.get();
      }
    }
  }
}
