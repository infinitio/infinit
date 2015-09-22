#include <cmath>
#include <sstream>
#include <fstream>

#include <openssl/sha.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <elle/container/list.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/json/exceptions.hh>
#include <elle/Exception.hh>
#include <elle/json/json.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <elle/print.hh>
#include <elle/system/platform.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <reactor/scheduler.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/EscapedString.hh>

#include <infinit/oracles/meta/Client.hh>
#include <infinit/oracles/meta/Error.hh>
#include <infinit/oracles/meta/macro.hh>
#include <infinit/oracles/meta/error/AccountLimitationError.hh>

#include <surface/gap/Error.hh>

#include <version.hh>

ELLE_LOG_COMPONENT("infinit.oracles.meta.Client");

/*-------------.
| Transactions |
`-------------*/

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      static
      std::string
      hash(std::string const& to_hash)
      {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX context;
        if (SHA256_Init(&context) == 0 ||
            SHA256_Update(&context, to_hash.c_str(), to_hash.size()) == 0 ||
            SHA256_Final(hash, &context) == 0)
          throw elle::Exception("Unable to hash password");
        std::ostringstream out;
        elle::serialize::OutputHexadecimalArchive ar(out);
        ar.SaveBinary(hash, SHA256_DIGEST_LENGTH);
        return out.str();
      }

      static
      std::string
      old_salt(std::string const& email,
               std::string const& password)
      {
        return email + "MEGABIET" + password + email + "MEGABIET";
      }

      static
      std::string
      salt(std::string const& password)
      {
        auto p = "Pe9chee|" + password;
        return p;
      }

      std::string
      old_password_hash(std::string const& email,
                        std::string const& password)
      {
        return hash(old_salt(email, password));
      }

      static
      std::string
      password_hash(std::string const& password)
      {
        return hash(salt(password));
      }

      /*--------.
      | Helpers |
      `--------*/

      static
      boost::posix_time::time_duration
      _requests_timeout()
      {
        auto env = elle::os::getenv("INFINIT_META_REQUEST_TIMEOUT", "");
        if (!env.empty())
          return boost::posix_time::seconds(boost::lexical_cast<int>(env));
        else
          return 30_sec;
      }

      static
      bool
      watermark(reactor::http::Request const& request)
      {
        static const std::string header = "X-Fist-Meta-Version";
        return request.headers().find(header) != request.headers().end();
      }

      static
      void
      _check_account_limit(std::string const& url,
                           reactor::http::Request& request)
      {
        ELLE_DEBUG_SCOPE("check if account has been limited (%s)", url);
        if (request.status() == reactor::http::StatusCode::Payment_Required)
        {
          try
          {
            elle::serialization::json::SerializerIn input(request, false);
            int32_t error_code;
            input.serialize("error", error_code);
            auto const& meta_error = Error(error_code);
            if (meta_error == Error::link_storage_limit_reached)
            {
              uint64_t quota;
              uint64_t usage;
              input.serialize("quota", quota);
              input.serialize("usage", usage);
              throw LinkQuotaExceeded(meta_error, quota, usage);
            }
            else if (meta_error == Error::send_to_self_limit_reached)
            {
              uint64_t limit;
              input.serialize("limit", limit);
              throw SendToSelfTransactionLimitReached(meta_error, limit);
            }
            else if (meta_error == Error::file_transfer_size_limited)
            {
              uint64_t limit;
              input.serialize("limit", limit);
              throw TransferSizeLimitExceeded(meta_error, limit);
            }
            else
            {
              std::string reason;
              input.serialize("reason", reason);
              throw AccountLimitationError(meta_error, reason);
            }
          }
          catch (elle::serialization::Error const& e)
          {
            ELLE_ERR("JSON parse error requesting %s: %s", url, e);
            if (e.inner_exception())
              ELLE_ERR("Inner exception: %s", *e.inner_exception());
            ELLE_ERR("body was: %s", request.response().string());
            throw;
          }
        }
      }

      /*-----.
      | Self |
      `-----*/

      Self::Self(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Self::serialize(elle::serialization::Serializer& s)
      {
        User::serialize(s);
        s.serialize("devices", this->devices);
        s.serialize("email", this->email);
        s.serialize("facebook_id", this->facebook_id);
        s.serialize("favorites", this->favorites);
        s.serialize("language", this->language);
        s.serialize("identity", this->identity);
      }

      std::string
      Self::identifier() const
      {
        if (this->email)
          return this->email.get();
        if (this->facebook_id)
          return this->facebook_id.get();
        return "";
      }

      /*-------------.
      | ServerStatus |
      `-------------*/

      ServerStatus::ServerStatus(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      ServerStatus::ServerStatus(bool status_, std::string message_)
        : status(status_)
        , message(std::move(message_))
      {}

      void
      ServerStatus::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("status", this->status);
        if (!this->status)
          s.serialize("message", this->message);
      }

      /*---------.
      | Fallback |
      `---------*/

      Fallback::Fallback(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Fallback::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("fallback_host", this->fallback_host);
        s.serialize("fallback_port_ssl", this->fallback_port_ssl);
        s.serialize("fallback_port_tcp", this->fallback_port_tcp);
      }

      /*-----.
      | User |
      `-----*/

      User::User(std::string const& id,
                 std::string const& fullname,
                 std::string const& handle,
                 std::string const& register_status,
                 User::Devices const& devices,
                 std::string const& public_key):
        id(id),
        fullname(fullname),
        handle(handle),
        register_status(register_status),
        connected_devices(devices),
        public_key(public_key)
      {}

      /*-------------.
      | SerializerIn |
      `-------------*/

      class SerializerIn
        : public elle::serialization::json::SerializerIn
      {
      public:
        SerializerIn(std::string const& url, reactor::http::Request& input)
        try
          : elle::serialization::json::SerializerIn(input, false)
        {}
        catch (elle::serialization::Error const& e)
        {
          ELLE_ERR("GIVE THE FOLLOWING ERROR TO MEFYL RIGHT AWAY. THX.");
          ELLE_ERR("https://app.asana.com/0/5058254180090/12894049383956");
          ELLE_ERR("JSON parse error requesting %s: %s", url, e);
          if (e.inner_exception())
            ELLE_ERR("Inner exception: %s", *e.inner_exception());
          ELLE_ERR("body was: %s", input.response().string());
          throw;
        }
      };

      using elle::sprintf;
      using reactor::http::Method;

      namespace json = elle::format::json;

      /*-------------.
      | Construction |
      `-------------*/

      static
      std::tuple<std::string, std::string, int>
      _parse_meta_url(std::string const& meta)
      {
        boost::regex const re("(?:([a-z]*)://)?([^:]+)(?::([0-9]+))?");
        boost::smatch res;
        if (!boost::regex_match(meta, res, re))
          throw elle::Error(
            elle::sprintf("unable to parse meta url: %s", meta));
        std::string scheme = "https";
        std::string host = res.str(2);
        int port = 443;
        if (res[1].matched)
          scheme = res.str(1);
        if (res[3].matched)
          try
          {
            port = boost::lexical_cast<int>(res.str(3));
          }
          catch (boost::bad_lexical_cast const&)
          {
            throw elle::Error(elle::sprintf("invalid port: %s", res.str(3)));
          }
        return std::make_tuple(
          std::move(scheme), std::move(host), std::move(port));
      }

      Client::Client(std::string const& meta)
        : Client(_parse_meta_url(meta))
      {}

      Client::Client(std::string const& protocol,
                     std::string const& server,
                     uint16_t port)
        : Client(std::make_tuple(protocol, server, port))
      {
        // Don't check the host name since we have only one certificate but we
        // change hostname between minor versions.
        this->_default_configuration.ssl_verify_host(false);
        this->_rng.seed(static_cast<unsigned int>(std::time(0)));
      }

      Client::Client(std::tuple<std::string, std::string, int> meta)
        : _protocol(std::move(std::get<0>(meta)))
        , _host(std::move(std::get<1>(meta)))
        , _port(std::move(std::get<2>(meta)))
        , _root_url(elle::sprintf("%s://%s:%d",
                                  this->_protocol, this->_host, this->_port))
        , _client(elle::os::getenv("INFINIT_USER_AGENT",
                                   "MetaClient/"
                                   INFINIT_VERSION
        " ("
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
      ")"))
        ,  _default_configuration(_requests_timeout(),
                                 {},
                                 reactor::http::Version::v10)
        , _email()
        , _logged_in(false)
      {
        ELLE_TRACE_SCOPE("%s: bound to %s", *this, this->_root_url);
        // Don't check the host name since we have only one certificate but we
        // change hostname between minor versions.
        this->_default_configuration.ssl_verify_host(false);
        this->_rng.seed(static_cast<unsigned int>(std::time(0)));
      }

      Client::~Client()
      {}

      std::string
      Client::session_id() const
      {
        if (this->_client.cookies().find("session-id") !=
          this->_client.cookies().end())
          return this->_client.cookies()["session-id"];
        else
          return ""; // FIXME: optionals, FFS
      }

      void
      Client::session_id(std::string const& session_id)
      {
        ELLE_TRACE_SCOPE("%s: set the session id to %s", *this, session_id);
        this->_default_configuration.cookies()["session-id"] = session_id;
      }

      /*------.
      | Users |
      `------*/

      User::User(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      User::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("id", this->id);
        s.serialize("fullname", this->fullname);
        s.serialize("handle", this->handle);
        s.serialize("register_status", this->register_status);
        s.serialize("connected_devices", this->connected_devices);
        s.serialize("public_key", this->public_key);
        s.serialize("ghost_code", this->ghost_code);
        s.serialize("ghost_profile", this->ghost_profile_url);
        s.serialize("phone_number", this->phone_number);
      }

      LoginResponse::LoginResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      LoginResponse::Trophonius::Trophonius(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      LoginResponse::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("self", this->self);
        s.serialize("device", this->device);
        s.serialize("trophonius", this->trophonius);
        s.serialize("features", this->features);
        s.serialize("account_registered", this->account_registered);
        s.serialize("ghost_code", this->ghost_code);
        s.serialize("referral_code", this->referral_code);
      }

      void
      LoginResponse::Trophonius::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("host", this->host);
        s.serialize("port", this->port);
        s.serialize("port_ssl", this->port_ssl);
      }

      WebLoginTokenResponse::WebLoginTokenResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      WebLoginTokenResponse::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("login-token", this->_token);
      }

      SynchronizeResponse::SynchronizeResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      SynchronizeResponse::serialize(elle::serialization::Serializer& s)
      {
        this->transactions.clear();
        std::vector<PeerTransaction> transaction_list;
        // Get the running transactions.
        s.serialize("running_transactions", transaction_list);
        for (auto const& tr: transaction_list)
          this->transactions.emplace(tr.id, tr);
        transaction_list.clear();
        // Get the final transactions (overwrite duplicates as Meta's response
        // has a race condition).
        s.serialize("final_transactions", transaction_list);
        for (auto const& tr: transaction_list)
          this->transactions[tr.id] = tr;
        s.serialize("links", this->links);
        s.serialize("swaggers", this->swaggers);
        s.serialize("devices", this->devices);
        s.serialize("accounts", this->external_accounts);
        s.serialize("account", this->account);
      }

      LoginResponse
      Client::login(std::string const& email,
                    std::string const& password,
                    boost::uuids::uuid const& device_uuid,
                    boost::optional<std::string> device_push_token,
                    boost::optional<std::string> country_code,
                    boost::optional<std::string> device_model,
                    boost::optional<std::string> device_name,
                    boost::optional<std::string> device_language)
      {
        ELLE_TRACE_SCOPE("%s: login as %s on device %s",
                         *this, email, device_uuid);
        this->_log_device(device_push_token, country_code,
                          device_model, device_name, device_language);
        elle::SafeFinally set_email([&] { this->_email = email; });
        try
        {
          return this->_login(
            [&] (elle::serialization::json::SerializerOut& parameters)
            {
              auto old_password = old_password_hash(email, password);
              ELLE_DUMP("old password hash: %s", old_password);
              auto new_password = password_hash(password);
              ELLE_DUMP("new password hash: %s", new_password);
              parameters.serialize("email", const_cast<std::string&>(email));
              parameters.serialize(
                "password", const_cast<std::string&>(old_password));
              parameters.serialize(
                "password_hash", const_cast<std::string&>(new_password));
            },
            device_uuid,
            device_push_token,
            country_code,
            device_model,
            device_name,
            device_language);
        }
        catch (...)
        {
          set_email.abort();
          throw;
        }
      }

      WebLoginTokenResponse
      Client::web_login_token() const
      {
        std::string url("/user/login-token");
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        return WebLoginTokenResponse(input);
      }

      LoginResponse
      Client::facebook_connect(
        std::string const& facebook_token,
        boost::uuids::uuid const& device_uuid,
        boost::optional<std::string> preferred_email,
        boost::optional<std::string> device_push_token,
        boost::optional<std::string> country_code,
        boost::optional<std::string> device_model,
        boost::optional<std::string> device_name,
        boost::optional<std::string> device_language,
        boost::optional<std::string> referral_code)
      {
        ELLE_TRACE_SCOPE("%s: login using facebook on device %s",
                         *this, device_uuid);
        this->_log_device(device_push_token, country_code,
                          device_model, device_name, device_language);
        return this->_login(
          [&] (elle::serialization::json::SerializerOut& parameters)
          {
            parameters.serialize("long_lived_access_token",
                                 const_cast<std::string&>(facebook_token));
            parameters.serialize("preferred_email", preferred_email);
          },
          device_uuid,
          device_push_token,
          country_code,
          device_model,
          device_name,
          device_language,
          referral_code);
      }

      bool
      Client::facebook_id_already_registered(
        std::string const& facebook_id_) const
      {
        reactor::http::EscapedString facebook_id{facebook_id_};
        auto url =
          elle::sprintf("/users/%s?account_type=facebook", facebook_id);
        auto request = this->_request(url, Method::GET, false);
        return request.status() == reactor::http::StatusCode::OK;
      }

      LoginResponse
      Client::_login(ParametersUpdater parameters_updater,
                     boost::uuids::uuid const& device_uuid,
                     boost::optional<std::string> device_push_token,
                     boost::optional<std::string> country_code,
                     boost::optional<std::string> device_model,
                     boost::optional<std::string> device_name,
                     boost::optional<std::string> device_language,
                     boost::optional<std::string> referral_code)
      {
        auto url = "/login";
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut output(r, false);
            output.serialize("device_push_token", device_push_token);
            output.serialize("country_code", country_code);
            parameters_updater(output);
            std::string struuid = boost::lexical_cast<std::string>(device_uuid);
            output.serialize("device_id", struuid);
            auto os = elle::system::platform::os_name();
            output.serialize("OS", os);
            output.serialize("device_model", device_model);
            output.serialize("device_name", device_name);
            output.serialize("device_language", device_language);
            output.serialize("referral_code", referral_code);
          },
          false);
        if (request.status() == reactor::http::StatusCode::Forbidden ||
            request.status() == reactor::http::StatusCode::Bad_Request ||
            request.status() == reactor::http::StatusCode::Service_Unavailable)
        {
          SerializerIn input(url, request);
          int error_code;
          input.serialize("code", error_code);
          using Error = infinit::oracles::meta::Error;
          switch (Error(error_code))
          {
            case Error::email_not_confirmed:
              throw infinit::state::UnconfirmedEmailError();
            case Error::email_password_dont_match:
              throw infinit::state::CredentialError();
            case Error::already_logged_in:
              throw infinit::state::AlreadyLoggedIn();
            case Error::deprecated:
              throw infinit::state::VersionRejected();
            case Error::email_not_valid:
              throw infinit::state::MissingEmail();
            case Error::email_already_registered:
              throw infinit::state::EmailAlreadyRegistered();
            case Error::maintenance_mode:
              throw infinit::state::MaintenanceMode();
            default:
              throw infinit::state::LoginError(
                elle::sprintf("%s: Unknown, good luck!", error_code));
          }
        }
        else
        {
          this->_handle_errors(request);
          {
            SerializerIn input(url, request);
            LoginResponse response(input);
            // Debugging purpose.
            this->_logged_in = true;
            return response;
          }
        }
      }

      void
      Client::add_facebook_account(std::string const& facebook_token) const
      {
        std::string url("/user/accounts_facebook");
        auto request = this->_request(
          url,
          Method::PUT,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut output(r, false);
            output.serialize("short_lived_access_token",
                             const_cast<std::string&>(facebook_token));
          });
      }

      void
      Client::logout()
      {
        this->_request("/logout", Method::POST);
        // Debugging purpose.
        this->_email = "";
        this->_logged_in = false;
      }

      LoginResponse::Trophonius
      Client::trophonius()
      {
        auto url = "/trophonius";
        auto request = this->_request(url, Method::GET, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::Forbidden:
            throw infinit::state::CredentialError();
          default:
            this->_handle_errors(request);
        }
        SerializerIn input(url, request);
        return LoginResponse::Trophonius(input);
      }

      RegisterResponse::RegisterResponse(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      RegisterResponse::serialize(elle::serialization::SerializerIn& s)
      {
        s.serialize("registered_user_id", this->id);
        s.serialize("ghost_code", this->ghost_code);
        s.serialize("referral_code", this->referral_code);
      }

      RegisterResponse
      Client::register_(std::string const& email,
                        std::string const& fullname,
                        std::string const& password,
                        boost::optional<std::string> referral_code) const
      {
        ELLE_TRACE_SCOPE("%s: register %s <%s>", *this, fullname, email);
        if (referral_code)
          ELLE_DEBUG("referral_code: %s", referral_code.get());
        std::string source = "app";
        auto url = "/user/register";
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            auto old_password = old_password_hash(email, password);
            ELLE_DUMP("old password hash: %s", old_password);
            auto new_password = password_hash(password);
            ELLE_DUMP("new password hash: %s", new_password);
            elle::serialization::json::SerializerOut output(request, false);
            output.serialize("email", const_cast<std::string&>(email));
            output.serialize("fullname", const_cast<std::string&>(fullname));
            output.serialize(
              "password", const_cast<std::string&>(old_password));
            output.serialize(
              "password_hash", const_cast<std::string&>(new_password));
            output.serialize("source", const_cast<std::string&>(source));
            if (referral_code)
            {
              output.serialize(
                "referral_code", const_cast<std::string&>(referral_code.get()));
            }
          });
        std::string user_id = "";
        SerializerIn input(url, request);
        bool success;
        input.serialize("success", success);
        if (success)
        {
          input.serialize("registered_user_id", user_id);
        }
        else
        {
          int error_code;
          input.serialize("error_code", error_code);
          using Error = infinit::oracles::meta::Error;
          switch(Error(error_code))
          {
            case Error::already_logged_in:
              throw infinit::state::AlreadyLoggedIn();
            case Error::email_already_registered:
              throw infinit::state::EmailAlreadyRegistered();
            case Error::email_not_valid:
              throw infinit::state::EmailNotValid();
            case Error::password_not_valid:
              throw infinit::state::PasswordNotValid();
            case Error::fullname_not_valid:
              throw infinit::state::FullnameNotValid();

            default:
              throw infinit::state::SelfUserError(
                elle::sprintf("Unknown registration error: %s", error_code));
          }
        }
        return RegisterResponse(input);
      }

      SynchronizeResponse
      Client::synchronize(bool init) const
      {
        std::string url = elle::sprintf("/user/synchronize?init=%s",
                                        init ? 1 : 0);
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        return SynchronizeResponse{input};
      }

      PlainInvitationResponse::PlainInvitationResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      PlainInvitationResponse::serialize(
        elle::serialization::Serializer& s)
      {
        s.serialize("identifier", this->_identifier);
        s.serialize("ghost_code", this->_ghost_code);
        s.serialize("shorten_ghost_profile_url", this->_ghost_profile_url);
      }

      PlainInvitationResponse
      Client::plain_invite_contact(std::string const& identifier) const
      {
        std::string url("/user/invite");
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut output(r, false);
            output.serialize("identifier",
                             const_cast<std::string&>(identifier));
          }, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::OK:
            break;
          case reactor::http::StatusCode::Bad_Request:
            throw infinit::state::InviteeInvalid();
          case reactor::http::StatusCode::Forbidden:
            throw infinit::state::InvitationError(request.response().string());
          default:
            this->_handle_errors(request);
        }
        SerializerIn input(url, request);
        return PlainInvitationResponse(input);
      }

      void
      Client::send_invite(std::string const& destination,
                          std::string const& message,
                          std::string const& ghost_code,
                          std::string const& invite_type,
                          bool user_cancel) const
      {
        auto request = this->_request(
          "/user/send_invite",
          Method::POST,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut output(r, false);
            output.serialize("destination",
                             const_cast<std::string&>(destination));
            output.serialize("message", const_cast<std::string&>(message));
            output.serialize("ghost_code",
                             const_cast<std::string&>(ghost_code));
            output.serialize("invite_type",
                             const_cast<std::string&>(invite_type));
            output.serialize("user_cancel", user_cancel);
          });
      }

      void
      Client::use_ghost_code(std::string const& code_) const
      {
        reactor::http::EscapedString code{code_};
        std::string url = elle::sprintf("/ghost/%s/merge", code);
        auto request = this->_request(url,
                                      Method::POST,
                                      false);
        switch (request.status())
        {
          case reactor::http::StatusCode::OK:
            break;
          case reactor::http::StatusCode::Gone:
            throw infinit::state::GhostCodeAlreadyUsed();
          default:
            throw infinit::state::InvalidGhostCode();
        }
      }

      bool
      Client::check_ghost_code(std::string const& code_) const
      {
        reactor::http::EscapedString code{code_};
        std::string url = elle::sprintf("/ghost/code/%s", code);
        auto request = this->_request(url, Method::GET, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::OK:
            return true;
          default:
            return false;
        }
      }

      static
      std::pair<std::string, User>
      email_and_user(boost::any const& json_)
      {
        auto const& json = boost::any_cast<elle::json::Object>(json_);
        std::pair<std::string, User> res;
        res.first = boost::any_cast<std::string>(json.at("email"));
        {
          elle::serialization::json::SerializerIn input(json_);
          res.second = User(input);
        }
        return res;
      }

      std::unordered_map<std::string, User>
      Client::search_users_by_emails(
        std::vector<std::string> const& emails, int limit, int offset) const
      {
        std::string url = elle::sprintf(
          "/user/search_emails?limit=%s&offset=%s", limit, offset);
        // Use a POST as otherwise the GET URL is too long to process.
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut query(r, false);
            query.serialize("emails",
                            const_cast<std::vector<std::string>&>(emails));
          });

        auto json = elle::json::read(request);
        auto const& users =
          boost::any_cast<elle::json::Array>(
            boost::any_cast<elle::json::Object>(json).at("users"));
        std::unordered_map<std::string, User> res;
        for (auto const& user: users)
        {
          res.insert(email_and_user(user));
        }
        return res;
      }

      std::vector<User>
      Client::users_search(std::string const& text, int limit, int offset) const
      {
        std::string url = "/users";
        reactor::http::Request::QueryDict query;
        query["search"] = text;
        if (limit > 0)
          query["limit"] = elle::sprintf("%s", limit);
        if (offset > 0)
          query["offset"] = elle::sprintf("%s", offset);
        auto request = this->_request(url, Method::GET, query);
        SerializerIn input(url, request);
        std::vector<User> res;
        input.serialize("users", res);
        return res;
      }

      User
      Client::user_from_handle(std::string const& handle) const
      {
        if (handle.size() == 0)
          throw elle::Exception("No handle");
        std::string url = elle::sprintf("/users/from_handle/%s", handle);
        auto request = this->_request(url, Method::GET, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::Not_Found:
            throw infinit::state::UserNotFoundError(handle);
          default:
            this->_handle_errors(request);
        }
        SerializerIn input(url, request);
        return User(input);
      }

      User
      Client::user(std::string const& recipient_identifier) const
      {
        if (recipient_identifier.size() == 0)
          throw elle::Exception("Invalid id or email");
        reactor::http::EscapedString identifier(recipient_identifier);
        std::string url = elle::sprintf("/users/%s", identifier);
        auto request = this->_request(url, Method::GET, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::Not_Found:
            throw infinit::state::UserNotFoundError(recipient_identifier);
          default:
            this->_handle_errors(request);
        }
        SerializerIn input(url, request);
        return User(input);
      }

      Self
      Client::self() const
      {
        auto url = "/user/self";
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        return Self(input);
      }

      std::vector<User>
      Client::get_swaggers() const
      {
        std::string url = "/user/swaggers";
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        std::vector<User> res;
        input.serialize("swaggers", res);
        return res;
      }

      void
      Client::remove_swagger(std::string const& id) const
      {
        this->_request(
          "/user/remove_swagger",
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut query(request, false);
            query.serialize("_id", const_cast<std::string&>(id));
          });
      }

      void
      Client::favorite(std::string const& user) const
      {
        this->_request(
          "/user/favorite",
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut query(request, false);
            query.serialize("user_id",
                            const_cast<std::string&>(user));
          });
      }

      void
      Client::unfavorite(std::string const& user) const
      {
        this->_request(
          "/user/unfavorite",
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut query(request, false);
            query.serialize("user_id",
                            const_cast<std::string&>(user));
          });
      }

      /*--------.
      | Devices |
      `--------*/

      std::vector<Device>
      Client::devices() const
      {
        std::string url = "/user/devices";
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        std::vector<Device> res;
        input.serialize("devices", res);
        return res;
      }

      Device
      Client::update_device(elle::UUID const& device_uuid,
                            boost::optional<std::string> const& name,
                            boost::optional<std::string> const& model,
                            boost::optional<std::string> const& os) const
      {
        std::string url = elle::sprintf("/devices/%s", device_uuid);
        auto request = this->_request(
          url,
          Method::PUT,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut query(request, false);
            if (name)
              query.serialize("name", const_cast<std::string&>(name.get()));
            if (model)
              query.serialize("model", const_cast<std::string&>(model.get()));
            if (os)
              query.serialize("os", const_cast<std::string&>(os.get()));
          });
        SerializerIn input(url, request);
        return Device(input);
      }

      Device
      Client::device(boost::uuids::uuid const& device_id) const
      {
        std::string const url = elle::sprintf("/devices/%s", device_id);
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        return Device(input);
      }

      /*------------------.
      | Peer Transactions |
      `------------------*/

      CreatePeerTransactionResponse::CreatePeerTransactionResponse(
        User const& recipient,
        PeerTransaction const& transaction,
        boost::optional<int32_t> status_info)
          : _recipient(recipient)
          , _status_info(status_info)
          , _transaction(transaction)
      {}

      CreatePeerTransactionResponse::CreatePeerTransactionResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      CreatePeerTransactionResponse::serialize(
        elle::serialization::Serializer& s)
      {
        s.serialize("recipient", this->_recipient);
        s.serialize("status_info_code", this->_status_info);
        s.serialize("transaction", this->_transaction);
      }

      UpdatePeerTransactionResponse::UpdatePeerTransactionResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      UpdatePeerTransactionResponse::serialize(
        elle::serialization::Serializer& s)
      {
        s.serialize("aws_credentials", this->_cloud_credentials);
        s.serialize("ghost_code", this->_ghost_code);
        s.serialize("ghost_profile", this->_ghost_profile_url);
        s.serialize("paused", this->_paused);
      }

      std::string
      Client::create_transaction(std::string const& recipient_identifier,
                                 std::list<std::string> const& files,
                                 uint64_t count,
                                 std::string const& message) const
      {
        ELLE_TRACE_SCOPE(
          "%s: create barebones peer transaction to %s",
          *this,
          recipient_identifier);
        std::string const url = "/transactions";
        auto method = Method::POST;
        auto request = this->_request(
          url,
          method,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut query(r, false);
            query.serialize("recipient_identifier",
                            const_cast<std::string&>(recipient_identifier));
            query.serialize("files",
                            const_cast<std::list<std::string>&>(files));
            int64_t count_integral = static_cast<int64_t>(count);
            query.serialize("files_count", count_integral);
            query.serialize("message", const_cast<std::string&>(message));
          }, false);
        _check_account_limit(url, request);
        this->_handle_errors(request);
        SerializerIn input(url, request);
        PeerTransaction transaction;
        input.serialize("transaction", transaction);
        return transaction.id;
      }

      CreatePeerTransactionResponse
      Client::fill_transaction(
        std::string const& recipient_identifier,
        std::list<std::string> const& files,
        uint64_t count,
        uint64_t size,
        bool is_dir,
        elle::UUID const& device_id,
        std::string const& message,
        std::string const& transaction_id,
        boost::optional<elle::UUID> recipient_device_id
        ) const
      {
        ELLE_TRACE_SCOPE(
          "%s: create peer transaction to %s%s",
          *this,
          recipient_identifier,
          recipient_device_id
          ? elle::sprintf(" on device %s", recipient_device_id.get()) : "");
        std::string url = elle::sprintf("/transaction/%s",transaction_id);
        auto request = this->_request(
          url,
          Method::PUT,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut query(r, false);
            query.serialize("recipient_identifier",
                            const_cast<std::string&>(recipient_identifier));
            query.serialize("files",
                            const_cast<std::list<std::string>&>(files));
            int64_t count_integral = static_cast<int64_t>(count);
            query.serialize("files_count", count_integral);
            int64_t size_integral = static_cast<int64_t>(size);
            query.serialize("total_size", size_integral);
            query.serialize("is_directory", is_dir);
            query.serialize("device_id", const_cast<elle::UUID&>(device_id));
            query.serialize("message", const_cast<std::string&>(message));
            query.serialize("recipient_device_id", recipient_device_id);
          }, false);
        _check_account_limit(url, request);
        this->_handle_errors(request);
        SerializerIn input(url, request);
        return CreatePeerTransactionResponse(input);
      }

      UpdatePeerTransactionResponse
      Client::update_transaction(std::string const& transaction_id,
                                 boost::optional<Transaction::Status> status,
                                 elle::UUID const& device_id,
                                 std::string const& device_name,
                                 boost::optional<bool> paused) const
      {
        ELLE_TRACE("%s: update %s transaction with new status %s",
                   *this,
                   transaction_id,
                   status);
        if (paused)
          ELLE_TRACE("%s: change transaction %s paused status to %s",
                     *this,
                     transaction_id,
                     *paused);
        auto url = "/transaction/update";
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& r)
          {
            elle::serialization::json::SerializerOut query(r, false);
            // FIXME: get rid of const casts when output serialization has his
            // const API
            query.serialize("transaction_id",
                            const_cast<std::string&>(transaction_id));
            if (status)
            {
              int status_integral = static_cast<int>(*status);
              query.serialize("status", status_integral);
              if (*status == oracles::Transaction::Status::accepted ||
                  *status == oracles::Transaction::Status::rejected)
              {
                ELLE_ASSERT(!device_id.is_nil());
                query.serialize("device_id",
                                const_cast<elle::UUID&>(device_id));
                query.serialize("device_name",
                                const_cast<std::string&>(device_name));
              }
            }
            query.serialize("paused", paused);
          });
        SerializerIn input(url, request);
        return UpdatePeerTransactionResponse(input);
      }

      PeerTransaction
      Client::transaction(std::string const& _id) const
      {
        std::string url = sprintf("/transaction/%s", _id);
        reactor::http::Request request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        return PeerTransaction(input);
      }

      std::vector<PeerTransaction>
      Client::transactions(std::vector<Transaction::Status> const& statuses,
                           bool negate,
                           int count) const
      {
        std::string url;
        if (statuses.size() > 0)
        {
          std::stringstream filter;
          std::vector<boost::any> status_json;
          for (auto s: statuses)
            status_json.push_back(boost::any(int(s)));
          bool with_endl = false;
          elle::json::write(filter, status_json, with_endl);
          // FIXME: handle query parameters in reactor Request
          url = elle::sprintf("/transactions?count=%d&negate=%d&filter=%s",
                              count, negate, filter.str());
        }
        else
          url = "/transactions";
        auto request = this->_request(url, Method::GET);
        std::list<PeerTransaction> res;
        SerializerIn input(url, request);
        input.serialize("transactions", res);
        return std::vector<PeerTransaction>(res.begin(), res.end());
      }

      void
      Client::transaction_endpoints_put(
        std::string const& transaction_id,
        elle::UUID const& device_id,
        adapter_type const& local_endpoints,
        adapter_type const& public_endpoints) const
      {
        elle::json::Object json;
        json["device"] = boost::lexical_cast<std::string>(device_id);
        auto convert_endpoints = [&](adapter_type const& endpoints)
          {
            std::vector<boost::any> res;
            for (auto const& endpoint: endpoints)
            {
              elle::json::Object o;
              o["ip"] = endpoint.first;
              o["port"] = endpoint.second;
              res.push_back(std::move(o));
            }
            return res;
          };
        json["locals"] = convert_endpoints(local_endpoints);
        json["externals"] = convert_endpoints(public_endpoints);
        auto url = elle::sprintf("/transaction/%s/endpoints", transaction_id);
        this->_request(url, Method::PUT, [&] (reactor::http::Request& r)
                       {
                         elle::json::write(r, json);
                       });
      }

      Fallback
      Client::fallback(std::string const& id) const
      {
        auto url = elle::sprintf("/apertus/fallback/%s", id);
        auto request = this->_request(url, Method::GET);
        SerializerIn input(url, request);
        return Fallback(input);
      }

      std::unique_ptr<CloudCredentials>
      Client::get_cloud_buffer_token(std::string const& transaction_id,
                                     bool force_regenerate) const
      {
        auto url =
          elle::sprintf("/transaction/%s/cloud_buffer?force_regenerate=%s",
                        transaction_id, force_regenerate ? "true" : "false");
        auto request = this->_request(url, Method::GET, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::Gone:
            throw infinit::state::TransactionFinalized();
          default:
            this->_handle_errors(request);
        }
        SerializerIn input(url, request);
        std::unique_ptr<CloudCredentials> res;
        input.serialize_forward(res);
        return res;
      }

      /*------.
      | Links |
      `------*/

      CreateLinkTransactionResponse::CreateLinkTransactionResponse(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      CreateLinkTransactionResponse::serialize(
        elle::serialization::Serializer& s)
      {
        s.serialize("aws_credentials", this->_cloud_credentials);
        s.serialize("transaction", this->_transaction);
      }

      std::string
      Client::create_link() const
      {
        ELLE_TRACE("%s: create empty link", *this);
        std::string const url = "/link_empty";
        auto request =  this->_request(
          url, Method::POST, [&] (reactor::http::Request& request)
          {
            // Use that because we expect the meta answer to be json so we have
            // to talk to it in json... Don't ask.
            request << "{}";
          }, false);
        _check_account_limit(url, request);
        this->_handle_errors(request);
        SerializerIn input(url, request);
        std::string created_link_id;
        input.serialize("created_link_id", created_link_id);
        return created_link_id;
      }

      CreateLinkTransactionResponse
      Client::fill_link(LinkTransaction::FileList const& files,
                        std::string const& name,
                        std::string const& message,
                        bool screenshot,
                        std::string const& link_id) const
      {
        auto url = elle::sprintf("/link/%s", link_id);
        auto method = Method::PUT;
        auto request = this->_request(
          url, method,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut query(request, false);
            // FIXME: get rid of const casts when output serialization has his
            // const API
            query.serialize("name", const_cast<std::string&>(name));
            query.serialize("files",
                            const_cast<LinkTransaction::FileList&>(files));
            query.serialize("message", const_cast<std::string&>(message));
            query.serialize("screenshot", screenshot);
          },
          false);
        _check_account_limit(url, request);
        this->_handle_errors(request);
        SerializerIn input(url, request);
        return CreateLinkTransactionResponse(input);
      }

      void
      Client::update_link(std::string const& id,
                          boost::optional<double> progress,
                          boost::optional<Transaction::Status> status,
                          boost::optional<bool> pause) const
      {
        auto url = elle::sprintf("/link/%s", id);
        auto request = this->_request(
          url, Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::json::Object body;
            if (progress)
              body["progress"] = *progress;
            if (status)
            body["status"] = int(*status);
            if (pause)
              body["pause"] = *pause;
            elle::json::write(request, body);
          });
      }

      std::vector<LinkTransaction>
      Client::links(int offset,
                    int count,
                    bool include_expired) const
      {
        std::string url =
          elle::sprintf("/links?offset=%s&count=%s", offset, count);
        if (include_expired)
          url += "&include_expired=1";
        auto request = this->_request(url, Method::GET);
        std::vector<LinkTransaction> res;
        SerializerIn input(url, request);
        input.serialize("links", res);
        return res;
      }

      std::unique_ptr<CloudCredentials>
      Client::link_credentials(std::string const& id,
                               bool regenerate) const
      {
        std::string url = elle::sprintf("/link/%s/credentials", id);
        auto request = regenerate
          ? this->_request(
            url,
            Method::POST,
            [&] (reactor::http::Request& request)
            {
              // If we POST, we must include valid JSON otherwise we get a 400.
              elle::json::Object empty_obj;
              elle::json::write(request, empty_obj);
            },
            true)
          : this->_request(url, Method::GET);
        SerializerIn input(url, request);
        std::unique_ptr<CloudCredentials> res;
        input.serialize_forward(res);
        return res;
      }

      /*--------------.
      | Server Status |
      `--------------*/

      ServerStatus
      Client::server_status() const
      {
        try
        {
          std::string const url = "/status";
          auto request = this->_client.request(this->_url(url),
                                               Method::GET,
                                               this->_default_configuration);
          reactor::wait(request);
          if (!watermark(request))
            return ServerStatus(
              false,
              "no internet connectivity: check your proxy credentials");
          auto status = request.status();
          switch (status)
          {
            case reactor::http::StatusCode::OK:
            {
              SerializerIn input(url, request);
              return ServerStatus(input);
            }
            case reactor::http::StatusCode::Bad_Gateway:
            case reactor::http::StatusCode::Gateway_Timeout:
            case reactor::http::StatusCode::Internal_Server_Error:
            case reactor::http::StatusCode::Service_Unavailable:
            {
              return ServerStatus(false, "service temporarily unavailable");
            }
            default:
            {
              return ServerStatus(
                false,
                elle::sprintf("service temporarily unavailable: %s", status));
            }
          }
        }
        catch (reactor::http::Timeout const&)
        {
          return ServerStatus(
            false, "no internet connectivity: network timeout");
        }
        catch (reactor::http::ResolutionFailure const&)
        {
          return ServerStatus(
            false, "no internet connectivity: resolution failure");
        }
        catch (reactor::http::RequestError const& e)
        {
          return ServerStatus(false, elle::sprintf("unknown error: %s", e));
        }
      }

      /*----------.
      | Self User |
      `----------*/

      void
      Client::change_email(std::string const& email,
                           std::string const& password) const
      {
        auto url = "/user/change_email_request";
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request, false);
            output.serialize("new_email", const_cast<std::string&>(email));
            auto hashed_password = password_hash(password);
            output.serialize("password", hashed_password);
          },
          false);
        switch (request.status())
        {
          case reactor::http::StatusCode::Forbidden:
          {
            SerializerIn input(url, request);
            int error_code;
            input.serialize("code", error_code);
            using Error = infinit::oracles::meta::Error;
            switch (Error(error_code))
            {
              case Error::email_already_registered:
                throw infinit::state::EmailAlreadyRegistered();
              case Error::password_not_valid:
                throw infinit::state::CredentialError();
              default:
                throw infinit::state::SelfUserError(
                  elle::sprintf("%s: Unknown, good luck!", error_code));
            }
          }
          default:
            this->_handle_errors(request);
        }
      }

      void
      Client::change_password(std::string const& old_password,
                              std::string const& password) const
      {
        auto url = "/user/change_password";
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            auto current_password = old_password_hash(this->email(), old_password);
            auto new_password = old_password_hash(this->email(), password);
            auto new_password_hash = password_hash(password);
            elle::serialization::json::SerializerOut output(request, false);
            output.serialize("old_password",
                             const_cast<std::string&>(current_password));
            output.serialize("new_password",
                             const_cast<std::string&>(new_password));
            output.serialize("new_password_hash",
                             const_cast<std::string&>(new_password_hash));
          });
        SerializerIn input(url, request);
        bool success;
        input.serialize("success", success);
        if (!success)
        {
          int error_code;
          input.serialize("error_code", error_code);
          using Error = infinit::oracles::meta::Error;
          switch (Error(error_code))
          {
            case Error::password_not_valid:
              throw infinit::state::CredentialError();
            default:
              throw infinit::state::SelfUserError(elle::sprintf(
                "Unknown change password error: %s", error_code));
          }
        }
      }

      void
      Client::edit_user(std::string const& fullname,
                         std::string const& handle) const
      {
        auto url = "/user/edit";
        auto request = this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request, false);
            output.serialize("fullname", const_cast<std::string&>(fullname));
            output.serialize("handle", const_cast<std::string&>(handle));
          });
        SerializerIn input(url, request);
        bool success;
        input.serialize("success", success);
        if (!success)
        {
          int error_code;
          input.serialize("error_code", error_code);
          using Error = infinit::oracles::meta::Error;
          switch (Error(error_code))
          {
            case Error::handle_already_registered:
              throw infinit::state::HandleAlreadyRegistered();
            default:
              throw infinit::state::SelfUserError(
                elle::sprintf("%s: Unknown, good luck!", error_code));
          }
        }
      }

      elle::Buffer
      Client::icon(std::string const& user_id) const
      {
        std::string url = elle::sprintf("/user/%s/avatar?no_place_holder=1",
                                        user_id);
        reactor::http::Request request =
          this->_request(url, Method::GET, false);
        switch (request.status())
        {
          case reactor::http::StatusCode::Not_Found:
            // If there is no avatar on the server, return an empty buffer.
            return elle::Buffer();
          default:
            this->_handle_errors(request);
        }
        return request.response();
      }

      void
      Client::icon(elle::ConstWeakBuffer const& icon) const
      {
        std::string content_type = "application/octet-stream";
        this->_request(
          "/user/avatar",
          reactor::http::Method::POST,
          reactor::http::Request::QueryDict(),
          Sender([&icon] (reactor::http::Request& request)
          {
            request.write((char const*) icon.contents(), icon.size());
          }),
          content_type);
      }

      /*--------.
      | Helpers |
      `--------*/

      void
      Client::_pacify_retry(int& try_count) const
      {
        ++try_count;
        // FIXME: make this delay exponential.
        boost::random::uniform_int_distribution<> random(100, 120);
        auto randomness = random(this->_rng) / 100.;
        auto factor = std::pow(2, std::min(try_count, 5));
        auto delay = 1_sec * factor * randomness;
        ELLE_TRACE("%s: wait %s before retrying", *this, delay);
        reactor::sleep(delay);
      }

      reactor::http::Request
      Client::_request(
        std::string const& url,
        reactor::http::Method method,
        bool throw_on_status) const
      {
        return this->_request(
          url, method,
          reactor::http::Request::QueryDict(),
          boost::optional<Sender>(),
          boost::optional<std::string>(),
          throw_on_status);
      }

      reactor::http::Request
      Client::_request(
        std::string const& url,
        reactor::http::Method method,
        Sender const& send,
        bool throw_on_status) const
      {
        return this->_request(
          url,
          method,
          reactor::http::Request::QueryDict(),
          send,
          boost::optional<std::string>(),
          throw_on_status);
      }

      reactor::http::Request
      Client::_request(
        std::string const& url,
        Method method,
        reactor::http::Request::QueryDict const& query_dict,
        boost::optional<Sender> const& send,
        boost::optional<std::string> const& content_type_,
        bool throw_on_status) const
      {
        int retry_count = 0;
        while (true)
        {
          try
          {
            // FIXME: The content-type should not be deduced from the method but
            // from whether there is a send method.
            std::string content_type;
            if (content_type_)
              content_type = content_type_.get();
            else if (send)
              content_type = "application/json";
            reactor::http::Request request =
              content_type.empty() ?
              this->_client.request(this->_url(url),
                                    method,
                                    this->_default_configuration) :
              this->_client.request(this->_url(url),
                                    method,
                                    content_type,
                                    this->_default_configuration);
            if (!query_dict.empty())
              request.query_string(query_dict);
            if (send)
              send.get()(request);
            auto status = request.status();
            switch (status)
            {
              case reactor::http::StatusCode::OK:
              {
                if (!watermark(request))
                {
                  ELLE_WARN("%s: response requesting %s "
                            "not coming from meta, retrying",
                            *this, url);
                  this->_pacify_retry(retry_count);
                  continue;
                }
                break;
              }
              case reactor::http::StatusCode::Bad_Gateway:
              case reactor::http::StatusCode::Gateway_Timeout:
              case reactor::http::StatusCode::Internal_Server_Error:
              case reactor::http::StatusCode::Service_Unavailable:
              {
                ELLE_WARN("%s: transient status %s requesting %s, retrying",
                          *this, status, url);
                this->_pacify_retry(retry_count);
                continue;
              }
              default:
              {
                if (!watermark(request))
                {
                  ELLE_WARN("%s: status %s requesting %s"
                            " not coming from meta, retrying",
                            *this, status, url);
                  this->_pacify_retry(retry_count);
                  continue;
                }
                if (throw_on_status)
                {
                  ELLE_ERR("%s: error while posting: %s", *this, status);
                  ELLE_ERR("%s: %s", *this, request.response().string());
                  ELLE_ERR("%s", elle::Backtrace::current());
                  if (status == reactor::http::StatusCode::Found)
                  {
                    auto it = request.headers().find("Location");
                    if (it != request.headers().end())
                      ELLE_WARN("%s: Found location is %s", *this, *it);
                    else
                      ELLE_WARN("%s: Found location header is missing", *this);
                  }
                  this->_handle_errors(request);
                }
              }
            }
            return request;
          }
          catch (reactor::http::Timeout const&)
          {
            ELLE_WARN("%s: timeout requesting %s, retrying", *this, url);
          }
          catch (reactor::http::ResolutionFailure const&)
          {
            ELLE_WARN("%s: resolution failure requesting %s, retrying",
                      *this, url);
            this->_pacify_retry(retry_count);
          }
          catch (reactor::http::RequestError&)
          {
            ELLE_ERR("%s: unhandled error requesting %s, retrying: %s",
                     *this, url, elle::exception_string());
            this->_pacify_retry(retry_count);
          }
        }
      }

      std::string
      Client::_url(std::string const& url) const
      {
        return this->_root_url + url;
      }

      void
      Client::_handle_errors(reactor::http::Request& request) const
      {
        auto response = request.status();
        elle::SafeFinally finally(
          [&]
          {
            if (this->_error_handlers.find(response) != this->_error_handlers.end())
            {
              this->_error_handlers.at(response)(request.url());
            }
          });
        if (response != reactor::http::StatusCode::OK)
        {
          ELLE_ERR("%s: error while posting: %s.\n%s",
                   *this, response, request.response().string());
          ELLE_ERR("%s", elle::Backtrace::current());
          throw elle::http::Exception(
            static_cast<elle::http::ResponseCode>(response),
            elle::sprintf("error %s performing %s on %s",
                          response, request.method(), request.url()));
        }
      }

      void
      Client::_log_device(boost::optional<std::string> push_token,
                          boost::optional<std::string> country,
                          boost::optional<std::string> model,
                          boost::optional<std::string> name,
                          boost::optional<std::string> language)
      {
        auto opt_to_str = [] (boost::optional<std::string>& opt) -> std::string
          {
            if (opt)
              return opt.get();
            return "<none>";
          };
        ELLE_TRACE("%s: device: country(%s) push_token(%s) model(%s) name(%s) "
                   "language(%s)",
                   *this, opt_to_str(country), opt_to_str(push_token),
                   opt_to_str(model), opt_to_str(name), opt_to_str(language));
      }

      void
      Client::upload_address_book(std::string const& json) const
      {
        std::string url = "/user/contacts";
        auto request = this->_request(url, Method::PUT,
          [&] (reactor::http::Request& request)
          {
            request << "{\"contacts\":" << json << "}";
          });
      }

      void
      Client::upload_address_book(
        std::vector<AddressBookContact> contacts) const
      {
        std::string url = "/user/contacts";
        auto request = this->_request(url, Method::PUT,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request, false);
            output.serialize("contacts", contacts);
          });
      }

      void
      User::print(std::ostream& stream) const
      {
        stream << "User(" << this->id << ", " << this->handle << ")";
      }

      /*----------.
      | Printable |
      `----------*/
      void
      Client::print(std::ostream& stream) const
      {
        stream << "meta::Client(" << this->_host << ":" << this->_port << " @" << this->_email << ")";
      }

      CloudCredentialsAws::CloudCredentialsAws(
        std::string const& access_key_id,
        std::string const& secret_access_key,
        std::string const& session_token,
        std::string const& region,
        std::string const& bucket,
        std::string const& folder,
        boost::posix_time::ptime expiration,
        boost::posix_time::ptime server_time)
        : aws::Credentials(access_key_id, secret_access_key, session_token,
                           region, bucket, folder, expiration, server_time)
      {}


      CloudCredentialsAws::CloudCredentialsAws(elle::serialization::SerializerIn& s)
        : aws::Credentials(s)
      {}

      void
      CloudCredentialsAws::serialize(elle::serialization::Serializer& s)
      {
        aws::Credentials::serialize(s);
      }
      CloudCredentials*
      CloudCredentialsAws::clone() const
      {
        return new CloudCredentialsAws(*this);
      }
      CloudCredentials*
      CloudCredentialsGCS::clone() const
      {
        return new CloudCredentialsGCS(*this);
      }
      CloudCredentialsGCS::CloudCredentialsGCS(elle::serialization::SerializerIn& s)
      {
        serialize(s);
      }
      void
      CloudCredentialsGCS::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("url", this->_url);
        s.serialize("expiration", this->_expiry);
        s.serialize("current_time", this->_server_time);
      }

      static const elle::serialization::Hierarchy<CloudCredentials>::
      Register<CloudCredentialsAws>
      _register_AwsCredentials("aws");
      static const elle::serialization::Hierarchy<CloudCredentials>::
      Register<CloudCredentialsGCS>
      _register_GcsCredentials("gcs");
    }

  }
}
