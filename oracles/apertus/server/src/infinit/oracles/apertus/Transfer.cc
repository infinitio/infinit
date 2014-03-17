#include <infinit/oracles/apertus/Transfer.hh>

#include <infinit/oracles/apertus/Apertus.hh>

#include <reactor/network/exception.hh>
#include <reactor/network/socket.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

#include <elle/finally.hh>
#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.oracles.apertus.Transfer");

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      Transfer::Transfer(Apertus& owner,
                         Apertus::TID tid,
                         Socket&& left,
                         Socket&& right):
        Waitable(elle::sprintf("transfer %s", tid)),
        _apertus(owner),
        _tid(tid),
        _left(std::move(left)),
        _right(std::move(right)),
        _forward(*reactor::Scheduler::scheduler(),
                 elle::sprintf("forward: %s", this->_tid),
                 [this] { this->_run(); })
      {
        ELLE_TRACE_SCOPE("%s: creation", *this);
      }

      Transfer::~Transfer()
      {
        ELLE_TRACE_SCOPE("%s: destruction", *this);
        this->_forward.terminate_now(false);
        this->_left.reset();
        this->_right.reset();
      }

      void
      Transfer::_handle(Socket& lhs, Socket& rhs)
      {
        static const uint32_t buff_size = 1024 * 1024 * 16;

        std::string sbuff(buff_size, 0);
        char* buff = &sbuff[0];
        reactor::network::Buffer recv(buff, buff_size);

        while (true)
        {
          ELLE_TRACE("wait for stuff to be readable");
          ELLE_ASSERT(lhs != nullptr);
          uint32_t size = lhs->read_some(recv);
          elle::ConstWeakBuffer send(buff, size);
          ELLE_DEBUG("%s: read %s bytes from %s", this->_tid, size, *lhs);
          if (!send.empty())
          {
            ELLE_DUMP("%s", send);
            ELLE_ASSERT(rhs != nullptr);
            rhs->write(send);
            ELLE_DEBUG("%s: write %s bytes to %s", this->_tid, size, *rhs);

            this->_apertus.add_to_bandwidth(size);
          }
        }
      }

      void
      Transfer::_run()
      {
        elle::SafeFinally pop(
          [this]
          {
            ELLE_TRACE("%s: pop myself from apertus", *this);
            this->_apertus._transfer_remove(*this);
            this->_signal();
          });

        ELLE_ASSERT(this->_left != nullptr);
        ELLE_ASSERT(this->_right != nullptr);

        try
        {
          elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
          {
            scope.run_background(
              elle::sprintf("%s: ltr", this->_tid),
              [this]
              {
                this->_handle(this->_left, this->_right);
              });

            scope.run_background(
              elle::sprintf("%s: rtl", this->_tid),
              [this]
              {
                this->_handle(this->_right, this->_left);
              });

            scope.wait();
          };
        }
        catch (reactor::Terminate const&)
        {
          // If a termination his explicitly asked, the responsability of
          // deleting the object is given to the caller of the termination.
          pop.abort();
        }
        catch (reactor::network::ConnectionClosed const&)
        {
          ELLE_LOG("%s: connection closed by peer", *this);
        }
        catch (...)
        {
          ELLE_ERR("%s: swallowed exception: %s", *this, elle::exception_string());
        }
      }

      /*----------.
      | Printable |
      `----------*/
      void
      Transfer::print(std::ostream& stream) const
      {
        stream << "Transfer: " << this->_tid << " "
               << this->_left.get() << " - " << this->_right.get();
      }
    }
  }
}
