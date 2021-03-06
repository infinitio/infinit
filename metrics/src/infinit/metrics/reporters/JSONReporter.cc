#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <elle/container/map.hh>
#include <elle/system/platform.hh>

#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

#include <infinit/metrics/reporters/JSONReporter.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("infinit.metrics.JSONReporter");

namespace infinit
{
  namespace metrics
  {
    JSONReporter::JSONReporter(std::string const& name,
                               reactor::http::StatusCode expected_status):
      Reporter::Reporter(name),
      _transaction_dest("transactions"),
      _user_dest("users"),
      _ui_dest("ui"),
      _expected_status(expected_status)
    {}

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    JSONReporter::_transaction_accepted(std::string const& transaction_id,
                                        bool onboarding)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("accepted");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::onboarding)] = onboarding;

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_connected(
      std::string const& transaction_id,
      std::string const& connection_method,
      int attempt)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("connected");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::connection_method)] = connection_method;
      data[this->_key_str(JSONKey::attempt_number)] = attempt;

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_link_transaction_created(std::string const& transaction_id,
                                            std::string const& sender_id,
                                            int64_t file_count,
                                            int64_t total_size,
                                            uint32_t message_length,
                                            bool screenshot,
                                            std::vector<std::string> extensions,
                                            bool onboarding)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("created");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::sender_id)] = sender_id;
      data[this->_key_str(JSONKey::file_count)] = file_count;
      data[this->_key_str(JSONKey::total_size)] = total_size;
      data[this->_key_str(JSONKey::message_length)] = message_length;
      data[this->_key_str(JSONKey::screenshot)] = screenshot;
      data[this->_key_str(JSONKey::onboarding)] = onboarding;
      data[this->_key_str(JSONKey::transaction_type)] =
        this->_transaction_type_str(LinkTransaction);
      data[this->_key_str(JSONKey::extensions)] =
        elle::json::Array(extensions.begin(), extensions.end());

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_peer_transaction_created(std::string const& transaction_id,
                                            std::string const& sender_id,
                                            std::string const& recipient_id,
                                            int64_t file_count,
                                            int64_t total_size,
                                            uint32_t message_length,
                                            bool ghost,
                                            bool onboarding,
                                            std::vector<std::string> extensions
                                            )
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("created");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::sender_id)] = sender_id;
      data[this->_key_str(JSONKey::recipient_id)] = recipient_id;
      data[this->_key_str(JSONKey::file_count)] = file_count;
      data[this->_key_str(JSONKey::total_size)] = total_size;
      data[this->_key_str(JSONKey::message_length)] = message_length;
      data[this->_key_str(JSONKey::ghost)] = ghost;
      data[this->_key_str(JSONKey::transaction_type)] =
        this->_transaction_type_str(PeerTransaction);
      data[this->_key_str(JSONKey::onboarding)] = onboarding;
      data[this->_key_str(JSONKey::extensions)] =
        elle::json::Array(extensions.begin(), extensions.end());

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_ended(
      std::string const& transaction_id,
      infinit::oracles::Transaction::Status status,
      std::string const& info,
      bool onboarding,
      bool by_user)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("ended");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::how_ended)] =
        this->_transaction_status_str(status);
      data[this->_key_str(JSONKey::message)] = info;
      data[this->_key_str(JSONKey::onboarding)] = onboarding;
      data[this->_key_str(JSONKey::by_user)] = by_user;
      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_deleted(std::string const& transaction_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("deleted");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;

      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_transfer_begin(std::string const& transaction_id,
                                             TransferMethod method,
                                              float initialization_time,
                                              int attempt)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("transfer_begin");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::transfer_method)] =
        this->_transfer_method_str(method);
      data[this->_key_str(JSONKey::initialization_time)] = initialization_time;
      data[this->_key_str(JSONKey::attempt_number)] = attempt;
      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_transaction_transfer_end(std::string const& transaction_id,
                                            TransferMethod method,
                                            float duration,
                                            uint64_t bytes_transfered,
                                            TransferExitReason reason,
                                            std::string const& message,
                                            int attempt)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("transfer_end");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::transfer_method)] =
        this->_transfer_method_str(method);
      data[this->_key_str(JSONKey::duration)] = duration;
      data[this->_key_str(JSONKey::bytes_transfered)] = bytes_transfered;
      data[this->_key_str(JSONKey::exit_reason)] =
        this->_transfer_exit_reason_str(reason);
      data[this->_key_str(JSONKey::message)] = message;
      data[this->_key_str(JSONKey::attempt_number)] = attempt;
      this->_send(this->_transaction_dest, data);
    }

    void
    JSONReporter::_aws_error(std::string const& transaction_id,
                             std::string const& operation,
                             std::string const& url,
                             unsigned int attempt_number,
                             int http_status,
                             std::string const& aws_error_code,
                             std::string const& message)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("aws_error");
      data[this->_key_str(JSONKey::transaction_id)] = transaction_id;
      data[this->_key_str(JSONKey::operation)] = operation;
      data[this->_key_str(JSONKey::url)] = url;
      data[this->_key_str(JSONKey::attempt_number)] = attempt_number;
      data[this->_key_str(JSONKey::http_status)] = http_status;
      data[this->_key_str(JSONKey::aws_error_code)] = aws_error_code;
      data[this->_key_str(JSONKey::message)] = message;
      this->_send(this->_transaction_dest, data);
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    JSONReporter::_user_favorite(std::string const& user_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/favorite");
      data[this->_key_str(JSONKey::who)] = user_id;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_login(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/login");
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(JSONKey::fail_reason)] = info;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_facebook_connect(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/facebook_connect");
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(JSONKey::fail_reason)] = info;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_logout(bool success, std::string const& info)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/logout");
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(JSONKey::fail_reason)] = info;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_register(bool success, std::string const& info,
                                 std::string const& with,
                                 boost::optional<std::string> ghost_code,
                                 boost::optional<std::string> referral_code)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        with.empty()
        ? std::string{"app/register"}
        : elle::sprintf("app/%s_register", with);
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (!success)
        data[this->_key_str(JSONKey::fail_reason)] = info;
      if (ghost_code)
        data[this->_key_str(JSONKey::ghost_code)] = ghost_code.get();
      if (referral_code)
        data[this->_key_str(JSONKey::referral_code)] = referral_code.get();
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_unfavorite(std::string const& user_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/unfavorite");
      data[this->_key_str(JSONKey::who)] = user_id;

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_heartbeat()
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/heartbeat");

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_first_launch()
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/first_launch");

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_changed_device_id(std::string const& old_device_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("app/change_device_id");
      data[this->_key_str(JSONKey::existing)] = old_device_id;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_proxy(reactor::network::ProxyType proxy_type)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/proxy");
      data[this->_key_str(JSONKey::proxy_type)] =
        std::string(elle::sprintf("%s", proxy_type));

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_crashed()
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/crashed");

      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_changed_download_dir(bool fallback)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("app/changed_download_dir");
      data[this->_key_str(JSONKey::fallback)] = fallback;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_added_ghost_code(std::string const& code, bool link)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/add_ghost_code");
      data[this->_key_str(JSONKey::ghost_code)] = code;
      data[this->_key_str(JSONKey::ghost_code_link)] = link;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_used_ghost_code(bool success,
                                        std::string const& code,
                                        bool link,
                                        std::string const& fail_reason)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/use_ghost_code");
      data[this->_key_str(JSONKey::ghost_code)] = code;
      data[this->_key_str(JSONKey::ghost_code_link)] = link;
      data[this->_key_str(JSONKey::fail_reason)] = fail_reason;
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_user_sent_invitation_message(
      bool success,
      std::string const& code,
      std::string const& method,
      std::string const& fail_reason,
      bool reminder)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("app/send_sms_ghost_code");
      data[this->_key_str(JSONKey::ghost_code)] = code;
      data[this->_key_str(JSONKey::method)] = method;
      data[this->_key_str(JSONKey::fail_reason)] = fail_reason;
      data[this->_key_str(JSONKey::status)] = this->_status_string(success);
      if (reminder)
        data[this->_key_str(JSONKey::reminder)] = reminder;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_link_quota_exceeded(uint64_t size,
                                       uint64_t used,
                                       uint64_t quota)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/quota_exceeded");
      data[this->_key_str(JSONKey::total_size)] = size;
      data[this->_key_str(JSONKey::used_storage)] = used;
      data[this->_key_str(JSONKey::quota)] = quota;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_send_to_self_limit_reached(uint64_t limit)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("app/send_to_self_limited");
      data[this->_key_str(JSONKey::limit)] = limit;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_file_transfer_limit_reached(uint64_t limit,
                                               uint64_t transfer_size)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("app/transfer_size_limited");
      data[this->_key_str(JSONKey::limit)] = limit;
      data[this->_key_str(JSONKey::total_size)] = transfer_size;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_ghost_download_limit_reached(std::string const& ghost_id)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] =
        std::string("app/ghost_download_limited");
      data[this->_key_str(JSONKey::ghost)] = ghost_id;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_performed_social_post(std::string const& medium)
    {
      elle::json::Object data;
      data[this->_key_str(JSONKey::event)] = std::string("app/social_post");
      data[this->_key_str(JSONKey::medium)] = medium;
      this->_send(this->_user_dest, data);
    }

    void
    JSONReporter::_ui(std::string const& event,
                      std::string const& from,
                      Additional const& additional)
    {
      ELLE_TRACE_SCOPE("%s: send ui metric: %s(%s) with extra arguments %s",
                       *this, event, from, additional);
      elle::json::Object data;
      for (auto const& pair: additional)
        data.emplace(pair.first, pair.second);
      data[this->_key_str(JSONKey::event)] = event;
      data["method"] = from;
      this->_send(this->_ui_dest, data);
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
      elle::json::Object version;
      version["major"] = INFINIT_VERSION_MAJOR;
      version["minor"] = INFINIT_VERSION_MINOR;
      version["subminor"] = INFINIT_VERSION_SUBMINOR;
      elle::json::Object infinit;
      infinit["version"] = std::move(version);
      // Should use elle::system::platform::name() but use this for backwards
      // compatibility.
      infinit["os"] = std::string(
#ifdef INFINIT_LINUX
        "Linux"
#elif defined(INFINIT_MACOSX)
        "OS X"
#elif defined(INFINIT_IOS)
        "iOS"
#elif defined(INFINIT_WINDOWS)
        "Windows"
#elif defined(INFINIT_ANDROID)
      "Android"
#else
# error "machine not supported"
#endif
         );
      infinit["os_version"] = elle::system::platform::os_version();
      data["infinit"] = std::move(infinit);
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
        if (!Reporter::metric_device_id().empty())
        {
          data[this->_key_str(JSONKey::device_id)] =
            Reporter::metric_device_id();
        }
        else
        {
          data[this->_key_str(JSONKey::device_id)] =
            std::string("unknown");
        }
        std::string plan = Reporter::metric_sender_plan().empty()
                         ? "unknown" : Reporter::metric_sender_plan();
        data[this->_key_str(JSONKey::plan)] = plan;
        std::string language = Reporter::metric_sender_language();
        if (!language.empty())
          data[this->_key_str(JSONKey::language)] = language;
        elle::json::Object feats;
        auto features = Reporter::metric_features();
        for (auto const& elem : Reporter::metric_features())
          feats[elem.first] = elem.second;
        data[this->_key_str(JSONKey::features)] = feats;
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

    void
    JSONReporter::_post(std::string const& destination,
                        elle::json::Object data)
    {
      auto url = this->_url(destination);
      ELLE_TRACE_SCOPE("%s: send event to %s", *this, url);
      ELLE_DUMP("%s: data: %s", *this, data);
      reactor::http::Request::Configuration cfg(
        10_sec, {}, reactor::http::Version::v11, true, this->proxy());
      cfg.expected_status(this->_expected_status);
      cfg.header_add("User-Agent", Reporter::user_agent());
      reactor::http::Request r(
        url, reactor::http::Method::POST, "application/json", cfg);
      elle::json::write(r, data);
      reactor::wait(r);
    }

    /*-----------------.
    | Helper Functions |
    `-----------------*/
    std::string
    JSONReporter::_key_str(JSONKey k)
    {
      switch (k)
      {
        case JSONKey::attempt_number:
          return "attempt_number";
        case JSONKey::aws_error_code:
          return "aws_error_code";
        case JSONKey::by_user:
          return "by_user";
        case JSONKey::bytes_transfered:
          return "bytes_transfered";
        case JSONKey::connection_method:
          return "connection_method";
        case JSONKey::device_id:
          return "device_id";
        case JSONKey::duration:
          return "duration";
        case JSONKey::event:
          return "event";
        case JSONKey::existing:
          return "existing";
        case JSONKey::exit_reason:
          return "exit_reason";
        case JSONKey::fail_reason:
          return "fail_reason";
        case JSONKey::fallback:
          return "fallback";
        case JSONKey::file_count:
          return "file_count";
        case JSONKey::ghost:
          return "ghost";
        case JSONKey::ghost_code:
          return "ghost_code";
        case JSONKey::ghost_code_link:
          return "link";
        case JSONKey::http_status:
          return "http_status";
        case JSONKey::how_ended:
          return "how_ended";
        case JSONKey::initialization_time:
          return "initialization_time";
        case JSONKey::language:
          return "language";
        case JSONKey::limit:
          return "limit";
        case JSONKey::medium:
          return "medium";
        case JSONKey::message:
          return "message";
        case JSONKey::message_length:
          return "message_length";
        case JSONKey::method:
          return "method";
        case JSONKey::onboarding:
          return "onboarding";
        case JSONKey::plan:
          return "plan";
        case JSONKey::proxy_type:
          return "proxy_type";
        case JSONKey::metric_sender_id:
          return "user";
        case JSONKey::operation:
          return "operation";
        case JSONKey::recipient_id:
          return "recipient";
        case JSONKey::referral_code:
          return "referral_code";
        case JSONKey::reminder:
          return "reminder";
        case JSONKey::screenshot:
          return "screenshot";
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
        case JSONKey::transaction_type:
          return "type";
        case JSONKey::transfer_method:
          return "transfer_method";
        case JSONKey::url:
          return "url";
        case JSONKey::user_agent:
          return "user_agent";
        case JSONKey::version:
          return "version";
        case JSONKey::who:
          return "who";
        case JSONKey::features:
          return "features";
        case JSONKey::quota:
          return "quota";
        case JSONKey::used_storage:
          return "used_storage";
        case JSONKey::extensions:
          return "extensions";
        default:
          ELLE_ABORT("invalid metrics JSON key: %s", k);
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
      return boost::lexical_cast<std::string>(status);
    }

    std::string
    JSONReporter::_transfer_method_str(TransferMethod method)
    {
      switch(method)
      {
        case TransferMethodP2P:
          return "peer-to-peer";
        case TransferMethodCloud:
          return "cloud-buffering";
        case TransferMethodGhostCloud:
          return "ghost-buffering";
        default:
          ELLE_ABORT("invalid trasnfer method: %s", method);
      }
    }

    std::string
    JSONReporter::_transfer_exit_reason_str(TransferExitReason reason)
    {
      switch(reason)
      {
        case TransferExitReasonFinished:
          return "finished";
        case TransferExitReasonExhausted:
          return "exhausted";
        case TransferExitReasonError:
          return "error";
        case TransferExitReasonTerminated:
          return "terminated";
        case TransferExitReasonUnknown:
          return "unknown";
        default:
          ELLE_ABORT("invalid exit reason: %s", reason);
      }
    }

    std::string
    JSONReporter::_transaction_type_str(TransactionType type)
    {
      switch (type)
      {
        case LinkTransaction:
          return "link";
        case PeerTransaction:
          return "peer";
        default:
          ELLE_ABORT("invalid transaction type: %s", type);
      }
    }
  }
}
