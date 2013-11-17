#include <infinit/oracles/apertus/Accepter.hh>
#include <infinit/oracles/apertus/Apertus.hh>

#include <reactor/network/tcp-socket.hh>
#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>

#include <elle/log.hh>
#include <elle/finally.hh>

ELLE_LOG_COMPONENT("infinit.oracles.apertus.Accepter");

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      Accepter::Accepter(Apertus& apertus,
                         std::unique_ptr<reactor::network::TCPSocket>&& client):
        _apertus(apertus),
        _client(std::move(client)),
        _accepter(
            *reactor::Scheduler::scheduler(),
            "accept",
            [this] { this->_handle(); }
            )
      {
        ELLE_TRACE_SCOPE("%s: construction", *this);
        ELLE_ASSERT(this->_client != nullptr);
      }

      Accepter::~Accepter()
      {
        ELLE_TRACE_SCOPE("%s: destruction", *this);
        this->_accepter.terminate_now(false);
      }

      void
      Accepter::_handle()
      {
        ELLE_TRACE_SCOPE("%s: start handling client", *this);
        elle::SafeFinally pop(
          [this]
          {
            ELLE_TRACE("%s: pop my self from apertus", *this);
            this->_apertus._accepter_remove(*this);
          });

        ELLE_ASSERT(this->_client != nullptr);

        try
        {
          // Retrieve TID size.
          char size;
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
          oracle::hermes::TID tid = std::string(tid_array);
          ELLE_DEBUG("%s: identifier: %s", *this, tid);

          // First client to connect with this TID, it must wait.
          if (this->_apertus._clients.find(tid) == this->_apertus._clients.end())
          {
            ELLE_TRACE("%s: first user for TID %s", *this, tid)
              this->_apertus._clients[tid] = this->_client.release();
          }
          // Second client, establishing connection.
          else
          {
            ELLE_TRACE("%s: peer found for TID %s", *this, tid)
            {
              auto* peer = this->_apertus._clients[tid];
              this->_apertus._clients.erase(tid);

              ELLE_DEBUG("%s: connect users", *this);
              this->_apertus._connect(
                tid,
                std::move(this->_client),
                std::move(std::unique_ptr<reactor::network::TCPSocket>(peer)));
            }
          }
        }
        catch (reactor::Terminate const&)
        {
          // If a termination his explicitly asked, the responsability of
          // deleting the object is given to the caller of the termination.
          ELLE_TRACE("%s: invalidate pop ward", *this);
          pop.abort();
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
