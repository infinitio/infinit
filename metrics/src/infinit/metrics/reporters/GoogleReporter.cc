#include <infinit/metrics/reporters/GoogleReporter.hh>

#include <elle/format/hexadecimal.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

#include <cryptography/Digest.hh>
#include <cryptography/oneway.hh>
#include <cryptography/Plain.hh>

ELLE_LOG_COMPONENT("infinit.metrics.GoogleReporter");

namespace infinit
{
  namespace metrics
  {
    GoogleReporter::GoogleReporter(bool investor_reporter):
      Reporter::Reporter("google reporter"),
      _base_url{"www.google-analytics.com"},
      _hashed_pkey{""},
      _investor_reporter(investor_reporter),
      _port(80)
    {
      if (this->_investor_reporter)
      {
#ifdef INFINIT_PRODUCTION_BUILD
        this->_tracking_id =
          elle::os::getenv("INFINIT_METRICS_INVESTORS_GOOGLE_TID",
                           "UA-31957100-2");
#else
        this->_tracking_id = "";
#endif // INFINIT_PRODUCTION_BUILD
        this->name("google investor reporter");
      }
      else
      {
#ifdef INFINIT_PRODUCTION_BUILD
        std::string default_tid = "UA-31957100-4";
#else
        std::string default_tid = "UA-31957100-5";
#endif // INFINIT_PRODUCTION_BUILD
        this->_tracking_id =
          elle::os::getenv("INFINIT_METRICS_GOOGLE_TID", default_tid);
      }
    }

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    GoogleReporter::_transaction_created(std::string const& transaction_id,
                                          std::string const& sender_id,
                                          std::string const& recipient_id,
                                          int64_t file_count,
                                          int64_t total_size,
                                          uint32_t message_length,
                                          bool invitation)
    {
      std::unordered_map<std::string, std::string> data;
      data[this->_key_str(GoogleKey::event)] =
        std::string("network:create:succeed");

      this->_send(data);
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    GoogleReporter::_user_login(bool success, std::string const& info)
    {
      this->_hashed_pkey =
        this->_create_pkey_hash(Reporter::metric_sender_id());
      std::unordered_map<std::string, std::string> data;
      if (success)
      {
        data[this->_key_str(GoogleKey::event)] = std::string("user:login");
        data[this->_key_str(GoogleKey::session)] = std::string("start");
        data[this->_key_str(GoogleKey::status)] = std::string("succeed");
      }
      else
      {
        data[this->_key_str(GoogleKey::event)] =
          std::string("user:login:failed");
        data[this->_key_str(GoogleKey::status)] = this->_status_string(success);
      }
      this->_send(data);
    }

    void
    GoogleReporter::_user_logout(bool success, std::string const& info)
    {
      std::unordered_map<std::string, std::string> data;
      data[this->_key_str(GoogleKey::event)] = std::string("user:logout");
      if (success)
      {
        data[this->_key_str(GoogleKey::session)] = std::string("end");
        data[this->_key_str(GoogleKey::status)] = this->_status_string(success);
      }
      else
      {
        data[this->_key_str(GoogleKey::status)] = this->_status_string(success);
      }

      this->_send(data);
    }

    /*-----------------.
    | Helper Functions |
    `-----------------*/
    std::string
    GoogleReporter::_create_pkey_hash(std::string const& id)
    {
      ELLE_TRACE_SCOPE("%s: creating hash from primary key %s", *this, id);
      std::string hashed_id;
      {
        elle::WeakBuffer id_buffer{(void*)id.data(), id.length()};
        using namespace infinit::cryptography;

        Digest digest{
          oneway::hash(Plain{id_buffer}, oneway::Algorithm::sha256)};
        hashed_id = elle::format::hexadecimal::encode(digest.buffer());
      }
      ELLE_DEBUG("%s: stage 1 hash: %s", *this, hashed_id);

      // Google user id must have the following format:
      // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxx
      // [[:alpha:]]{8}-[[:alpha:]]{4}-[[:alpha:]]{4}-[[:alpha:]]{4}-[[:alpha:]]{12}
      if (hashed_id.length() < 32)
        hashed_id.append(32 - hashed_id.length(), '6');
      else
        hashed_id.erase(32, std::string::npos);

      hashed_id
        .insert(20, "-")
        .insert(16, "-")
        .insert(12, "-")
        .insert(8, "-");

      ELLE_DEBUG("%s: generated google compliant hash: %s", *this, hashed_id);
      return hashed_id;
    }

    std::string
    GoogleReporter::_key_str(GoogleKey k)
    {
      switch (k)
      {
        case GoogleKey::api_version:
          return "v";
        case GoogleKey::application_name:
          return "an";
        case GoogleKey::client_id:
          return "cid";
        case GoogleKey::client_version:
          return "av";
        case GoogleKey::domain:
          return "dh";
        case GoogleKey::event:
          return "cd";
        case GoogleKey::interaction_type:
          return "t";
        case GoogleKey::session:
          return "cs";
        case GoogleKey::status:
          return "cd1";
        case GoogleKey::tracking_id:
          return "tid";
        default:
          elle::unreachable();
      }
    }

    std::string
    GoogleReporter::_make_param(bool first,
                                std::string const& parameter,
                                std::string const& value)
    {
      if (first)
      {
        if (!parameter.empty())
          return elle::sprintf("?%s=%s", parameter, value);
        else
          return elle::sprintf("?%s", value);
      }
      else
      {
        if (!parameter.empty())
          return elle::sprintf("&%s=%s", parameter, value);
        else
          return elle::sprintf("&%s", value);
      }
    }

    void
    GoogleReporter::_send(
      std::unordered_map<std::string, std::string>& data)
    {
      try
      {
        auto event_name =
          boost::any_cast<std::string>(data[this->_key_str(GoogleKey::event)]);
        if (this->_tracking_id.empty())
        {
          ELLE_LOG("%s: not sending '%s' metric to %s",
                   *this,
                   event_name,
                   this->name());
          return;
        }
        ELLE_TRACE_SCOPE("%s: sending metric: %s",
                         *this,
                         event_name);

        std::unordered_map<std::string, std::string> parameter_list(data);
        parameter_list[this->_key_str(GoogleKey::domain)] = "infinit.io";
        parameter_list[this->_key_str(GoogleKey::client_version)] =
          Reporter::client_version();
        parameter_list[this->_key_str(GoogleKey::application_name)] = "Infinit";
        parameter_list[this->_key_str(GoogleKey::interaction_type)] = "appview";
        parameter_list[this->_key_str(GoogleKey::client_id)] = this->_hashed_pkey;
        parameter_list[this->_key_str(GoogleKey::tracking_id)] =
          this->_tracking_id;
        parameter_list[this->_key_str(GoogleKey::api_version)] = "1";

        std::string parameters = this->_make_param(true, "", "payload_data");
        for (auto& p: parameter_list)
          parameters += this->_make_param(false, p.first, p.second);

        reactor::http::Request::Configuration cfg(10_sec);
        cfg.header_add("Content-Type", "application/x-www-form-urlencoded");
        cfg.header_add("User-Agent", Reporter::user_agent());

        auto url = elle::sprintf("http://%s:%s/collect%s",
                                 this->_base_url,
                                 this->_port,
                                 parameters);

        ELLE_DEBUG("%s: request URL for %s: %s", *this, this->name(), url);

        reactor::http::Request r(url,
                                 reactor::http::Method::GET,
                                 cfg);
        reactor::wait(r);
      }
      catch (reactor::http::RequestError const& e)
      {
        ELLE_WARN("%s: unable to post metric (%s): %s",
                  *this,
                  boost::any_cast<std::string>(
                    data[this->_key_str(GoogleKey::event)]),
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
    GoogleReporter::_status_string(bool success)
    {
      if (success)
        return "succeed";
      else
        return "failed";
    }
  }
}
