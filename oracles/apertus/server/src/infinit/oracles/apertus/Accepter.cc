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
        ELLE_ASSERT(this->_client != nullptr);
      }

      Accepter::~Accepter()
      {
        this->_accepter.terminate_now(false);
      }

      void
      Accepter::_handle()
      {
        elle::SafeFinally pop(
          [this]
          {
            ELLE_TRACE("pop");
            this->_apertus._accepter_remove(*this);
          });

        ELLE_ASSERT(this->_client != nullptr);

        try
        {
          // Retrieve TID size.
          char size;
          reactor::network::Buffer tmp(&size, 1);
          this->_client->read(tmp);

          // Retrieve TID of the client.
          char tid_array[size + 1];
          tid_array[size] = '\0';
          reactor::network::Buffer tid_buffer(tid_array, size);
          this->_client->read(tid_buffer);
          oracle::hermes::TID tid = std::string(tid_array);

          // First client to connect with this TID, it must wait.
          if (this->_apertus._clients.find(tid) == this->_apertus._clients.end())
          {
            ELLE_LOG("%s: first user for TID %s", *this, tid);
            this->_apertus._clients[tid] = this->_client.release();
          }
          // Second client, establishing connection.
          else
          {
            ELLE_LOG("%s: peer found for TID %s", *this, tid);
            auto* peer = this->_apertus._clients[tid];
            this->_apertus._clients.erase(tid);

            this->_apertus._connect(
              tid,
              std::move(this->_client),
              std::move(std::unique_ptr<reactor::network::TCPSocket>(peer)));
          }
        }
        catch (reactor::network::ConnectionClosed const&)
        {
          ELLE_LOG("%s: connection closed", *this);
        }
        catch (...)
        {
          ELLE_WARN("%s: lol", *this);
        }
      }
    }
  }
}
