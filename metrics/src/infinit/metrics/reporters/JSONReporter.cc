#include <functional>

#include <infinit/metrics/reporters/JSONReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

ELLE_LOG_COMPONENT("infinit.metrics.JSONReporter");

namespace infinit
{
  namespace metrics
  {
    JSONReporter::JSONReporter(std::string const& name):
      Reporter::Reporter(name),
      _transaction_dest("transactions"),
      _user_dest("users")
    {}

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    JSONReporter::_transaction_accepted(std::string const& transaction_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("transaction accepted");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_connected(
      std::string const& transaction_id,
      std::string const& connection_method)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("transaction connected");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::connection_method)] = connection_method;

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_created(std::string const& transaction_id,
                                          std::string const& sender_id,
                                          std::string const& recipient_id,
                                          int64_t file_count,
                                          int64_t total_size,
                                          uint32_t message_length)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("transaction created");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::sender_id)] = sender_id;
      data[this->_key_str(JSONKey::recipient_id)] = recipient_id;
      data[this->_key_str(JSONKey::file_count)] = file_count;
      data[this->_key_str(JSONKey::total_size)] = total_size;
      data[this->_key_str(JSONKey::message_length)] = message_length;

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_ended(
      std::string const& transaction_id,
      infinit::oracles::Transaction::Status status,
      std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("transaction ended");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::how_ended)] =
        this->_transaction_status_str(status);

      this->_send(this->_transaction_dest, data);
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    JSONReporter::_user_favorite(std::string const& user_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("user favorite");
      data[this->_key_str(JSONKey::who)] = user_id;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_login(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("user login");
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(JSONKey::fail_reason)] = info;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_logout(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("user logout");
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(JSONKey::fail_reason)] = info;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_unfavorite(std::string const& user_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("user unfavorite");
      data[this->_key_str(JSONKey::who)] = user_id;

      this->_send(this->_user_dest, data);
    }

    /*-----.
    | Send |
    `-----*/

    void
    JSONReporter::_send(std::string const& destination,
                        elle::json::Object data)
    {
      auto event_name =
        boost::any_cast<std::string>(data[this->_key_str(JSONKey::event)]);
      try
      {
        ELLE_TRACE_SCOPE("%s: sending metric: %s", *this, event_name);
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch()).count();
        data[this->_key_str(JSONKey::timestamp)] =
          boost::lexical_cast<double>(timestamp);
        data[this->_key_str(JSONKey::version)] = Reporter::client_version();
        if (!Reporter::metric_sender_id().empty())
        {
          data[this->_key_str(JSONKey::metric_sender_id)] =
            Reporter::metric_sender_id();
        }
        else
        {
          data[this->_key_str(JSONKey::metric_sender_id)] =
            std::string("unknown");
        }
        ELLE_DUMP("%s: json to be sent: %s",
                  *this, elle::json::pretty_print(data));
        this->_post(destination, data);
      }
      catch (reactor::http::RequestError const& e)
      {
        ELLE_WARN("%s: unable to post metric %s: %s", *this, event_name, e);
      }
      catch (reactor::Terminate const&)
      {
        throw;
      }
      catch (...)
      {
        ELLE_ERR("%s: unable to post metric %s: %s",
                 *this, event_name, elle::exception_string());
      }
    }

    /*-----------------.
    | Helper Functions |
    `-----------------*/
    std::string
    JSONReporter::_key_str(JSONKey k)
    {
      switch (k)
      {
        case JSONKey::connection_method:
          return "connection_method";
        case JSONKey::event:
          return "event";
        case JSONKey::fail_reason:
          return "fail_reason";
        case JSONKey::file_count:
          return "file_count";
        case JSONKey::how_ended:
          return "how_ended";
        case JSONKey::message_length:
          return "message_length";
        case JSONKey::metric_sender_id:
          return "user";
        case JSONKey::recipient_id:
          return "recipient";
        case JSONKey::sender_id:
          return "sender";
        case JSONKey::status:
          return "status";
        case JSONKey::timestamp:
          return "timestamp";
        case JSONKey::total_size:
          return "file_size";
        case JSONKey::transaction_id:
          return "transaction";
        case JSONKey::user_agent:
          return "user_agent";
        case JSONKey::version:
          return "version";
        case JSONKey::who:
          return "who";
        default:
          elle::unreachable();
      }
    }

    std::string
    JSONReporter::_status_string(bool success)
    {
      if (success)
        return "succeed";
      else
        return "failed";
    }

    std::string
    JSONReporter::_transaction_status_str(
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