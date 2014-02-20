#include <functional>

#include <infinit/metrics/reporters/InfinitReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

ELLE_LOG_COMPONENT("infinit.metrics.InfinitReporter");

namespace infinit
{
  namespace metrics
  {
    InfinitReporter::InfinitReporter():
      Reporter::Reporter("infinit reporter"),
      _transaction_dest{"transactions"},
      _user_dest{"users"}
    {
#ifdef INFINIT_PRODUCTION_BUILD
      std::string default_base_url = "v3.metrics.api.production.infinit.io";
#else
      std::string default_base_url = "v3.metrics.api.development.infinit.io";
#endif // INFINIT_PRODUCTION_BUILD
      this->_base_url = elle::os::getenv("INFINIT_METRICS_HOST",
                                   default_base_url);
      this->_port = boost::lexical_cast<int>(
        elle::os::getenv("INFINIT_METRICS_PORT", "80"));
    }

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    InfinitReporter::_transaction_accepted(std::string const& transaction_id)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] =
        std::string("transaction accepted");
      data[this->_key_str(InfinitKey::transaction_id)] = transaction_id;

      this->_send(this->_transaction_dest, data);
    }

    void
    InfinitReporter::_transaction_connected(
      std::string const& transaction_id,
      std::string const& connection_method)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] =
        std::string("transaction connected");
      data[this->_key_str(InfinitKey::transaction_id)] = transaction_id;
      data[this->_key_str(InfinitKey::connection_method)] = connection_method;

      this->_send(this->_transaction_dest, data);
    }

    void
    InfinitReporter::_transaction_created(std::string const& transaction_id,
                                          std::string const& sender_id,
                                          std::string const& recipient_id,
                                          int64_t file_count,
                                          int64_t total_size,
                                          uint32_t message_length)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] =
        std::string("transaction created");
      data[this->_key_str(InfinitKey::transaction_id)] = transaction_id;
      data[this->_key_str(InfinitKey::sender_id)] = sender_id;
      data[this->_key_str(InfinitKey::recipient_id)] = recipient_id;
      data[this->_key_str(InfinitKey::file_count)] = file_count;
      data[this->_key_str(InfinitKey::total_size)] = total_size;
      data[this->_key_str(InfinitKey::message_length)] = message_length;

      this->_send(this->_transaction_dest, data);
    }

    void
    InfinitReporter::_transaction_ended(
      std::string const& transaction_id,
      infinit::oracles::Transaction::Status status,
      std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] =
        std::string("transaction ended");
      data[this->_key_str(InfinitKey::transaction_id)] = transaction_id;
      data[this->_key_str(InfinitKey::how_ended)] =
        this->_transaction_status_str(status);

      this->_send(this->_transaction_dest, data);
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    InfinitReporter::_user_favorite(std::string const& user_id)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] = std::string("user favorite");
      data[this->_key_str(InfinitKey::who)] = user_id;

      this->_send(this->_user_dest, data);
    }

    void
    InfinitReporter::_user_login(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] = std::string("user login");
      data[this->_key_str(InfinitKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(InfinitKey::fail_reason)] = info;

      this->_send(this->_user_dest, data);
    }

    void
    InfinitReporter::_user_logout(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] = std::string("user logout");
      data[this->_key_str(InfinitKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(InfinitKey::fail_reason)] = info;

      this->_send(this->_user_dest, data);
    }

    void
    InfinitReporter::_user_unfavorite(std::string const& user_id)
    {
      elle::json::Object data;
      data[this->_key_str(InfinitKey::event)] = std::string("user unfavorite");
      data[this->_key_str(InfinitKey::who)] = user_id;

      this->_send(this->_user_dest, data);
    }

    /*-----------------.
    | Helper Functions |
    `-----------------*/
    std::string
    InfinitReporter::_key_str(InfinitKey k)
    {
      switch (k)
      {
        case InfinitKey::connection_method:
          return "connection_method";
        case InfinitKey::event:
          return "event";
        case InfinitKey::fail_reason:
          return "fail_reason";
        case InfinitKey::file_count:
          return "file_count";
        case InfinitKey::how_ended:
          return "how_ended";
        case InfinitKey::message_length:
          return "message_length";
        case InfinitKey::metric_sender_id:
          return "user";
        case InfinitKey::recipient_id:
          return "recipient";
        case InfinitKey::sender_id:
          return "sender";
        case InfinitKey::status:
          return "status";
        case InfinitKey::timestamp:
          return "timestamp";
        case InfinitKey::total_size:
          return "file_size";
        case InfinitKey::transaction_id:
          return "transaction";
        case InfinitKey::user_agent:
          return "user_agent";
        case InfinitKey::version:
          return "version";
        case InfinitKey::who:
          return "who";
        default:
          elle::unreachable();
      }
    }

    void
    InfinitReporter::_send(std::string const& destination,
                           elle::json::Object data)
    {
      try
      {
        auto event_name =
          boost::any_cast<std::string>(data[this->_key_str(InfinitKey::event)]);
        ELLE_TRACE_SCOPE("%s: sending metric: %s",
                         *this,
                         event_name);
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch()).count();

        data[this->_key_str(InfinitKey::timestamp)] =
          boost::lexical_cast<double>(timestamp);
        data[this->_key_str(InfinitKey::version)] = Reporter::client_version();
        if (!Reporter::metric_sender_id().empty())
        {
          data[this->_key_str(InfinitKey::metric_sender_id)] =
            Reporter::metric_sender_id();
        }
        else
        {
          data[this->_key_str(InfinitKey::metric_sender_id)] =
            std::string("unknown");
        }

        auto url = elle::sprintf("http://%s:%d/%s",
                                 this->_base_url,
                                 this->_port,
                                 destination);
        reactor::http::Request::Configuration cfg(10_sec,
                                                  reactor::http::Version::v11);
        cfg.header_add("User-Agent", Reporter::user_agent());
        ELLE_DEBUG("%s: json to be sent: %s",
                   *this,
                   elle::json::pretty_print(data));

        reactor::http::Request r(url,
                                 reactor::http::Method::POST,
                                 "application/json",
                                 cfg);
        elle::json::write(r, data);
        reactor::wait(r);
      }
      catch (reactor::http::RequestError const& e)
      {
        ELLE_WARN("%s: unable to post metric (%s): %s",
                  *this,
                  boost::any_cast<std::string>(
                    data[this->_key_str(InfinitKey::event)]),
                  e.what());
      }
      catch (reactor::Terminate const&)
      {
        ELLE_ERR("%s: caught reactor terminate", *this);
        throw;
      }
      catch (...)
      {
        ELLE_ERR("%s: exception while trying to post metric: %s",
                 *this,
                 elle::exception_string());
      }
    }

    std::string
    InfinitReporter::_status_string(bool success)
    {
      if (success)
        return "succeed";
      else
        return "failed";
    }

    std::string
    InfinitReporter::_transaction_status_str(
      infinit::oracles::Transaction::Status status)
    {
      switch (status)
      {
        case infinit::oracles::Transaction::Status::accepted:
          return "accepted";
        case infinit::oracles::Transaction::Status::canceled:
          return "cancelled";
        case infinit::oracles::Transaction::Status::created:
          return "created";
        case infinit::oracles::Transaction::Status::failed:
          return "failed";
        case infinit::oracles::Transaction::Status::finished:
          return "finished";
        case infinit::oracles::Transaction::Status::initialized:
          return "initialized";
        case infinit::oracles::Transaction::Status::none:
          return "none";
        case infinit::oracles::Transaction::Status::rejected:
          return "rejected";
        case infinit::oracles::Transaction::Status::started:
          return "started";
        default:
          elle::unreachable();
      }
    }
  }
}
