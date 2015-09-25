#include "server.hh"

#include <boost/lexical_cast.hpp>

#include <elle/Buffer.hh>
#include <elle/UUID.hh>
#include <elle/log.hh>
#include <elle/finally.hh>
#include <elle/container/map.hh>
#include <elle/serialization/json/SerializerIn.hh>
#include <elle/serialization/json.hh>

#include <infinit/oracles/meta/Client.hh>
#include <version.hh>

#include <fist/tests/_detail/Authority.hh>

ELLE_LOG_COMPONENT("fist.tests");

std::unique_ptr<papier::Identity>
generate_identity(infinit::cryptography::rsa::KeyPair const& keypair,
                  std::string const& id,
                  std::string const& description,
                  std::string const& password);

namespace tests
{
  void
  Server::register_route(std::string const& route,
                         reactor::http::Method method,
                         Super::Function const& function)
  {
    Super::register_route(route, method,
                          [this, function] (Super::Headers const& headers,
                                            Super::Cookies const& cookies,
                                            Super::Parameters const& parameters,
                                            elle::Buffer const& body)
                          {
                            auto it = this->headers().find("Set-Cookie");
                            if (it != this->headers().end())
                              this->headers().erase(it);
                            return function(headers, cookies, parameters, body);
                          });

  }

  infinit::oracles::meta::Account::Quotas
  Server::empty_account_quotas()
  {
    namespace meta_ns = infinit::oracles::meta;
    meta_ns::Account::Quotas quotas;
    meta_ns::Account::Quotas::QuotaUsage links;
    meta_ns::Account::Quotas::QuotaUsage send_to_self;
    meta_ns::Account::Quotas::Limit p2p;
    links.quota = 0;
    links.used = 0;
    send_to_self.quota = 0;
    send_to_self.used = 0;
    quotas.links = links;
    quotas.send_to_self = send_to_self;
    quotas.p2p = p2p;
    return quotas;
  }

  Server::Server(std::unique_ptr<Trophonius> trophonius)
    : _session_id(elle::UUID::random())
    , trophonius(trophonius ?
                 std::move(trophonius) : elle::make_unique<Trophonius>())
    , _cloud_buffered(false)
  {
    this->headers()["X-Fist-Meta-Version"] = INFINIT_VERSION;

    this->register_route(
      "/status",
      reactor::http::Method::GET,
      [] (Server::Headers const&,
          Server::Cookies const&,
          Server::Parameters const&,
          elle::Buffer const&)
      {
        return "{\"status\" : true}";
      });

    this->register_route(
      "/login",
      reactor::http::Method::POST,
      std::bind(&Server::_login_post,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4));

    this->register_route(
      "/trophonius",
      reactor::http::Method::GET,
      std::bind(&Server::_get_trophonius,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4));

    this->register_route(
      "/user/synchronize",
      reactor::http::Method::GET,
      [&] (Server::Headers const&,
           Server::Cookies const& cookies,
           Server::Parameters const& parameters,
           elle::Buffer const&)
      {
        User const& user = this->user(cookies);
        auto const& device = this->device(cookies);
        std::vector<std::unique_ptr<infinit::oracles::Transaction>> runnings;
        std::vector<std::unique_ptr<infinit::oracles::Transaction>> finals;
        for (auto& transaction: this->_transactions)
        {
          if (transaction->sender_id == user.id ||
              transaction->recipient_id == user.id)
          {
            auto const& final_statuses = transaction->sender_id == user.id
              ? surface::gap::Transaction::sender_final_statuses
              : surface::gap::Transaction::recipient_final_statuses;

            if (final_statuses.find(transaction->status) == final_statuses.end())
            {
              runnings.emplace_back(
                std::unique_ptr<infinit::oracles::PeerTransaction>(
                  new infinit::oracles::PeerTransaction(*transaction)));
            }
            else
            {
              finals.emplace_back(
                std::unique_ptr<infinit::oracles::PeerTransaction>(
                  new infinit::oracles::PeerTransaction(*transaction)));
            }
          }
        }
        std::stringstream res;
        {
          namespace meta_ns = infinit::oracles::meta;
          elle::serialization::json::SerializerOut output(res, false);
          std::vector<meta_ns::User> swaggers;
          for (auto const& u: user.swaggers)
            swaggers.emplace_back(*u);
          output.serialize("swaggers", swaggers);
          output.serialize("running_transactions", runnings);
          output.serialize("final_transactions", finals);
          std::vector<std::unique_ptr<infinit::oracles::Transaction>> links;
          for (auto& link: user.links)
          {
            links.emplace_back(
              std::unique_ptr<infinit::oracles::LinkTransaction>(
                new infinit::oracles::LinkTransaction(link.second)));
          }
          output.serialize("links", links);
          output.serialize("devices", std::vector<meta_ns::Device>{device});
          meta_ns::ExternalAccount email_account;
          email_account.id = user.email();
          email_account.type = "email";
          output.serialize(
            "accounts", std::vector<meta_ns::ExternalAccount>{email_account});
          infinit::oracles::meta::Account account;
          account.custom_domain = "";
          account.link_format =  std::string("http://%s/_/%s");
          account.quotas = this->empty_account_quotas();
          output.serialize("account", account);
        }
        ELLE_DEBUG("synchronize res: %s", res.str());
        return res.str();
      });

    this->register_route(
      "/logout",
      reactor::http::Method::POST,
      [] (Server::Headers const&,
          Server::Cookies const&,
          Server::Parameters const&,
          elle::Buffer const&)
      {
        return "{\"success\": true}";
      });

    this->register_route(
      "/user/self",
      reactor::http::Method::GET,
      [&] (Server::Headers const& header,
           Server::Cookies const& cookies,
           Server::Parameters const& parameters,
           elle::Buffer const& buffer)
      {
        auto id = this->user(cookies).id;
        return this->routes()[elle::sprintf("/user/%s", id)][reactor::http::Method::GET](
          header, cookies, parameters, buffer);
      });
    this->register_route(
      "/user/full_swaggers",
      reactor::http::Method::GET,
      [&] (Server::Headers const&,
           Server::Cookies const&,
           Server::Parameters const&,
           elle::Buffer const&)
      {
        return "{\"success\": true, \"swaggers\": []}";
      });
    this->register_route(
      "/transactions",
      reactor::http::Method::GET,
      [&] (Server::Headers const&,
           Server::Cookies const&,
           Server::Parameters const&,
           elle::Buffer const&)
      {
        return "{\"success\": true, \"transactions\": []}";
      });
    this->register_route(
      "/links",
      reactor::http::Method::GET,
      [&] (Server::Headers const&,
           Server::Cookies const&,
           Server::Parameters const&,
           elle::Buffer const&)
      {
        return "{\"success\": true, \"links\": []}";
      });

    this->register_route(
      "/link_empty",
      reactor::http::Method::POST,
      [&] (Server::Headers const&,
           Server::Cookies const& cookies,
           Server::Parameters const&,
           elle::Buffer const&)
      {
        User const& user = this->user(cookies);
        infinit::oracles::LinkTransaction t;
        t.id = boost::lexical_cast<std::string>(elle::UUID::random());
        auto id = t.id;
        struct InsertLink
        {
          InsertLink(infinit::oracles::LinkTransaction t)
            : t(t)
          {}

          void
          operator()(User& user)
          {
            user.links[t.id] = t;
          }

          infinit::oracles::LinkTransaction t;
        };
        this->_users.modify(this->_users.get<0>().find(user.id), InsertLink(t));

      this->register_route(
        elle::sprintf("/link/%s", id),
        reactor::http::Method::PUT,
        [&, id] (Server::Headers const&,
             Server::Cookies const& cookies,
             Server::Parameters const& parameters,
             elle::Buffer const& body)
        {
          User const& user = this->user(cookies);
          auto const& device = this->device(cookies);

          struct UpdateLink
          {
            UpdateLink(std::string const& id,
                       Device const& device)
              : id(id)
              , device(device)
            {}

            void
            operator()(User& user)
            {
              auto& t = user.links.at(this->id);
              t.click_count = 3;
              t.ctime = 2173213;
              t.sender_id = user.id;
              t.sender_device_id = device.id.repr();
              t.status = infinit::oracles::Transaction::Status::initialized;
            }
            std::string id;
            Device const& device;
          };
          this->_users.modify(this->_users.get<0>().find(user.id), UpdateLink(id, device));

          auto const& t = this->_users.get<0>().find(user.id)->links.at(id);
          auto now = boost::posix_time::second_clock::universal_time();
          auto tomorrow = now + boost::posix_time::hours(24);
          std::unique_ptr<infinit::oracles::meta::CloudCredentials> creds(
            new infinit::oracles::meta::CloudCredentialsAws(
              "", "", "", "us-east-1", "", id, tomorrow, now));
          std::stringstream res;
          {
            elle::serialization::json::SerializerOut output(res, false);
            std::unique_ptr<infinit::oracles::Transaction> serializable(
              new infinit::oracles::LinkTransaction(t));
            output.serialize("transaction", serializable);
            output.serialize("aws_credentials", creds);
          }
          ELLE_DEBUG("link PUT response: %s", res.str());
          return res.str();
        });

        this->register_route(
          elle::sprintf("/link/%s", id),
          reactor::http::Method::POST,
          [&, id] (Server::Headers const&,
                  Server::Cookies const& cookies,
                  Server::Parameters const& parameters,
                  elle::Buffer const& body)
          {
            elle::IOStream stream(body.istreambuf());
            elle::serialization::json::SerializerIn input(stream, false);
            int status;
            User const& user = this->user(cookies);
            struct UpdateLink
            {
              UpdateLink(std::string const& id,
                         infinit::oracles::Transaction::Status status)
                : id(id)
                , status(status)
              {}

              void
              operator()(User& user)
              {
                auto& t = user.links.at(this->id);
                t.status = this->status;
              }
              std::string id;
              infinit::oracles::Transaction::Status status;
            };
            input.serialize("status", status);
            this->_users.modify(
              this->_users.get<0>().find(
                user.id),
              UpdateLink(id,
                         static_cast<infinit::oracles::Transaction::Status>(status)));
            return "{\"success\": true}";
          });

        this->register_route(
          elle::sprintf("/s3/%s/filename", id),
          reactor::http::Method::POST,
          [&, id] (Server::Headers const&,
                  Server::Cookies const&,
                  Server::Parameters const& parameters,
                  elle::Buffer const&)
          {
            static bool b = true;
            elle::SafeFinally f([&] { b = false; });
            if (b)
              return std::string{
                "<InitiateMultipartUploadResult>"
                  "  <Bucket>bucket</Bucket>"
                  "  <Key>filename</Key>"
                  "  <UploadId>VXBsb2FkIElEIGZvciA2aWWpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA</UploadId>"
                  "</InitiateMultipartUploadResult>"};
            else
              return elle::sprintf(
                "<CompleteMultipartUploadResult>"
                "<Location></Location>"
                "<Bucket>bucket</Bucket>"
                "<Key>filename</Key>"
                "<ETag>%s</ETag>"
                "</CompleteMultipartUploadResult>",
                this->headers()["ETag"]);
          });

        this->register_route(
          elle::sprintf("/s3/%s/filename", id),
          reactor::http::Method::PUT,
          [&, id] (Server::Headers const&,
                  Server::Cookies const&,
                  Server::Parameters const& parameters,
                  elle::Buffer const&)
          {
            this->headers()["ETag"] = boost::lexical_cast<std::string>(elle::UUID::random());
            return elle::sprintf(
              "<CompleteMultipartUploadResult>"
              "<Location></Location>"
              "<Bucket>bucket</Bucket>"
              "<Key>filename</Key>"
              "<ETag>%s</ETag>"
              "</CompleteMultipartUploadResult>",
              this->headers()["ETag"]);
          });

        this->register_route(
          elle::sprintf("/s3/%s_data", id),
          reactor::http::Method::PUT,
          [] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const&)
          {
            return "";
          });

        this->register_route(
          elle::sprintf("/s3/%s/000000000000_0000", id),
          reactor::http::Method::PUT,
          [this] (Server::Headers const&,
                  Server::Cookies const&,
                  Server::Parameters const&,
                  elle::Buffer const&)
          {
            return "";
          });

        return elle::sprintf(
          "{"
          "  \"created_link_id\": \"%s\""
          "}",
          id);
      });

    this->register_route(
      "/transactions",
      reactor::http::Method::POST,
      std::bind(&Server::_transaction_post,
                std::ref(*this),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4));

    this->register_route(
      "/s3/folder/cloud-buffered",
      reactor::http::Method::POST,
      [] (Server::Headers const&,
          Server::Cookies const&,
          Server::Parameters const& parameters,
          elle::Buffer const&)
      {
        if (contains(parameters, "uploads"))
          return
            "<InitiateMultipartUploadResult>"
            "  <UploadId>upload-id</UploadId>"
            "</InitiateMultipartUploadResult>";
        else
          return
            "<CompleteMultipartUploadResult>"
            "</CompleteMultipartUploadResult>";
      });
    this->register_route(
      "/s3/folder/cloud-buffered",
      reactor::http::Method::PUT,
      [] (Server::Headers const&,
          Server::Cookies const&,
          Server::Parameters const&,
          elle::Buffer const&)
      {
        return "";
      });
    this->register_route(
      "/s3/folder/meta_data",
      reactor::http::Method::PUT,
      [this] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const& body)
      {
        ELLE_LOG("%s: store S3 metadata: %s", *this, body)
        this->_s3_meta_data = body.string();
        return "";
      });
    this->register_route(
      "/s3/folder/meta_data",
      reactor::http::Method::GET,
      [this] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const&)
      {
        if (!this->_s3_meta_data)
          throw reactor::http::tests::Server::Exception(
            "/s3/folder/meta_data",
            reactor::http::StatusCode::Not_Found,
            "no meta data");
        else
          return this->_s3_meta_data.get();
      });
    this->register_route(
      "/s3/folder/000000000000_0000",
      reactor::http::Method::PUT,
      [this] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const& body)
      {
        this->_maybe_sleep();
        this->_cloud_buffered = true;
        this->_s3_data = body.string();
        return "";
      });
    this->register_route(
      "/s3/folder/000000000000_0000",
      reactor::http::Method::GET,
      [this] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const&)
      {
        if (!this->_s3_data)
          throw reactor::http::tests::Server::Exception(
            "/s3/folder/000000000000_0000",
            reactor::http::StatusCode::Not_Found,
            "no data");
        else
          return this->_s3_data.get();
      });

    this->register_route(
      "/transaction/update",
      reactor::http::Method::POST,
      [this] (Server::Headers const&,
              Server::Cookies const& cookies,
              Server::Parameters const&,
              elle::Buffer const& content)
      {
        elle::IOStream stream(content.istreambuf());
        elle::serialization::json::SerializerIn input(stream, false);
        std::string id;
        boost::optional<int> status;
        boost::optional<bool> paused;
        input.serialize("transaction_id", id);
        input.serialize("status", status);
        input.serialize("paused", paused);
        ELLE_LOG_SCOPE("%s: update transaction \"%s\" to status %s",
                       *this, id, status);
        auto it = this->_transactions.find(id);
        if (it == this->_transactions.end())
        {
          ELLE_LOG("%s: transaction \"%s\" not found", *this, id)
          throw reactor::http::tests::Server::Exception(
            "/transaction/update",
            reactor::http::StatusCode::Not_Found,
            "transaction not found");
        }
        auto& t = **it;
        if (paused)
          t.paused = *paused;
        if (status)
        {
          t.status = infinit::oracles::Transaction::Status(*status);
          t.status_changed()(t.status);
          if (t.status == infinit::oracles::Transaction::Status::accepted)
          {
            this->headers()["ETag"] = boost::lexical_cast<std::string>(elle::UUID::random());
            auto const& user = this->user(cookies);
            auto const& device = this->device(cookies);
            t.recipient_id = user.id;
            t.recipient_device_id = device.id.repr();
          }
        }
        auto& tr = **this->_transactions.find(id);
        std::string transaction_notification = tr.json();
        transaction_notification.insert(1, "\"notification_type\":7,");
        for (auto& socket: this->trophonius->clients(elle::UUID(tr.sender_id)))
          socket->write(transaction_notification);
        for (auto& socket: this->trophonius->clients(elle::UUID(tr.recipient_id)))
          socket->write(transaction_notification);
        std::stringstream res;
        {
          elle::serialization::json::SerializerOut output(res, false);
          output.serialize("updated_transaction_id", tr.id);
          if (t.status == infinit::oracles::Transaction::Status::accepted)
          {
            output.serialize("recipent_device_id", tr.recipient_device_id);
            std::string name = "bite";
            output.serialize("recipient_device_name", name);
          }
        }
        return res.str();
      });
  }

  User const&
  Server::facebook_connect(std::string const& token,
                           bool& resgistered)
  {
    static std::unordered_map<std::string, std::string> facebook_ids;
    if (facebook_ids.find(token) == facebook_ids.end())
    {
      User const& user = this->register_user("", "");
      // Replace identity.
      auto keys =
        infinit::cryptography::rsa::keypair::generate(
          papier::Identity::keypair_length);
      std::unique_ptr<papier::Identity> identity{
        generate_identity(
          keys, user.id, "identity", "")};
      struct UpdateIdentity
      {
        UpdateIdentity(std::unique_ptr<papier::Identity>&& identity)
          : identity(std::move(identity))
        {}

        void
        operator()(User& user)
        {
          user.identity().reset(this->identity.release());
        }

        std::unique_ptr<papier::Identity> identity;
      };
      this->_users.modify(this->_users.get<0>().find(user.id), UpdateIdentity(std::move(identity)));
      facebook_ids[token] = user.facebook_id();
      resgistered = true;
    }
    auto const& facebook_id = facebook_ids[token];
    auto& users = this->_users.get<2>();
    auto it = users.find(facebook_id);
    if (it == users.end())
      throw reactor::http::tests::Server::Exception(
        "/login",
        reactor::http::StatusCode::Not_Found,
        "user not found");
    return *it;
  }

  elle::UUID
  Server::_create_empty(std::string const& sender_id,
                        std::string const& recipient_identifier)
  {
    User const& recipient = [&] () -> User const&
    {
      try
      {
        return this->user(recipient_identifier);
      }
      catch (Server::Exception const&)
      {
        return this->generate_ghost_user(recipient_identifier);
      }
    }();
    auto t = elle::make_unique<Transaction>(sender_id, recipient.id);
    std::string id = t->id;
    ELLE_LOG_SCOPE("%s: create transaction %s", *this, id);
    this->_transactions.insert(std::move(t));

    this->register_route(
      elle::sprintf("/transaction/%s", id),
      reactor::http::Method::PUT,
      std::bind(&Server::_transaction_put,
                std::ref(*this),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4,
                elle::UUID(id)));

    this->register_route(
      elle::sprintf("/transaction/%s", id),
      reactor::http::Method::GET,
      [&, id] (Server::Headers const&,
               Server::Cookies const& cookies,
               Server::Parameters const&,
               elle::Buffer const& content)
      {
        auto& tr = **this->_transactions.find(id);
        return tr.json();
      });

    this->register_route(
      elle::sprintf("/transaction/%s/endpoints", id),
      reactor::http::Method::PUT,
      [&, id] (Server::Headers const&,
               Server::Cookies const& cookies,
               Server::Parameters const&,
               elle::Buffer const& content)
      {
        auto const& device = this->device(cookies);
        User const& user = this->user(cookies);

        elle::IOStream stream(content.istreambuf());
        auto json = boost::any_cast<elle::json::Object>(elle::json::read(stream));
        auto locals = boost::any_cast<elle::json::Array>(json.at("locals"));
        auto sub_json = boost::any_cast<elle::json::Object>(locals[0]);
        auto port = boost::any_cast<int64_t>(sub_json.at("port"));

        static std::unordered_map<std::string, uint16_t> first_time;
        if (first_time.find(id) == first_time.end())
          first_time[id] = port;
        else
        {
          auto& tr = **this->_transactions.find(id);
          bool sender = tr.sender_device_id == device.id &&
            tr.sender_id == user.id;
          auto other_notif = elle::sprintf(
            "{"
            " \"notification_type\": 11,"
            " \"transaction_id\":\"%s\","
            " \"peer_endpoints\":{\"locals\":[{\"ip\":\"127.0.0.1\",\"port\":%s}],\"externals\":[]},"
            " \"devices\":[\"%s\", \"%s\"],"
            " \"status\":true"
            "}\n",
            id,
            port,
            tr.sender_device_id,
            tr.recipient_device_id);
          auto our_notif = elle::sprintf(
            "{"
            " \"notification_type\": 11,"
            " \"transaction_id\":\"%s\","
            " \"peer_endpoints\":{\"locals\":[{\"ip\":\"127.0.0.1\",\"port\":%s}],\"externals\":[]},"
            " \"devices\":[\"%s\", \"%s\"],"
            " \"status\":true"
            "}\n",
            id,
            first_time[id],
            tr.sender_device_id,
            tr.recipient_device_id);
          auto* to_sender = this->trophonius->socket(elle::UUID(tr.sender_id), tr.sender_device_id);
          auto* to_recipient = this->trophonius->socket(elle::UUID(tr.recipient_id), tr.recipient_device_id);
          if (to_sender)
          {
            to_sender->write(sender ? our_notif : other_notif);
          }
          if (to_recipient)
          {
            to_recipient->write(sender ? other_notif : our_notif);
          }
        }
        return "{}";
      });

    this->register_route(
      "/transaction/update",
      reactor::http::Method::POST,
      [this] (Server::Headers const&,
              Server::Cookies const& cookies,
              Server::Parameters const&,
              elle::Buffer const& content)
      {
        elle::IOStream stream(content.istreambuf());
        elle::serialization::json::SerializerIn input(stream, false);
        std::string id;
        boost::optional<int> status;
        boost::optional<bool> paused;
        input.serialize("transaction_id", id);
        input.serialize("status", status);
        input.serialize("paused", paused);
        ELLE_LOG_SCOPE("%s: update transaction \"%s\" to status %s",
                       *this, id, status);
        auto it = this->_transactions.find(id);
        if (it == this->_transactions.end())
        {
          ELLE_LOG("%s: transaction \"%s\" not found", *this, id)
          throw reactor::http::tests::Server::Exception(
            "/transaction/update",
            reactor::http::StatusCode::Not_Found,
            "transaction not found");
        }
        auto& tr = **this->_transactions.find(id);
        if (paused)
          tr.paused = *paused;
        if (status)
        {
          tr.status = infinit::oracles::Transaction::Status(*status);
          tr.status_changed()(tr.status);
          if (tr.status == infinit::oracles::Transaction::Status::accepted)
          {
            this->headers()["ETag"] =
              boost::lexical_cast<std::string>(elle::UUID::random());
            auto const& user = this->user(cookies);
            auto const& device = this->device(cookies);
            tr.recipient_id = user.id;
            tr.recipient_device_id = device.id.repr();
          }
          if (tr.status == infinit::oracles::Transaction::Status::cloud_buffered)
          {
            tr.cloud_buffered = true;
          }
        }
        std::string transaction_notification = tr.json();
        transaction_notification.insert(1, "\"notification_type\":7,");
        if (tr.sender_id.length())
        {
          for (auto& socket: this->trophonius->clients(elle::UUID(tr.sender_id)))
            socket->write(transaction_notification);
        }
        if (tr.recipient_id.length())
        {
          for (auto& socket: this->trophonius->clients(elle::UUID(tr.recipient_id)))
            socket->write(transaction_notification);
        }
        std::stringstream res;
        {
          elle::serialization::json::SerializerOut output(res, false);
          output.serialize("updated_transaction_id", tr.id);
          if (tr.status == infinit::oracles::Transaction::Status::accepted)
          {
            output.serialize("recipent_device_id", tr.recipient_device_id);
            std::string name = "bite";
            output.serialize("recipient_device_name", name);
          }
          if (tr.cloud_credentials())
          {
            output.serialize("aws_credentials", tr.cloud_credentials());
          }
        }
        return res.str();
      });

    this->register_route(
      elle::sprintf("/transaction/%s/cloud_buffer", id),
      reactor::http::Method::GET,
      [&, id] (Server::Headers const&,
               Server::Cookies const&,
               Server::Parameters const&,
               elle::Buffer const&)
      {
        auto now = boost::posix_time::second_clock::universal_time();
        auto tomorrow = now + boost::posix_time::hours(24);
        std::unique_ptr<infinit::oracles::meta::CloudCredentials> creds(
          new infinit::oracles::meta::CloudCredentialsAws(
            "", "", "", "region", "bucket", "folder", tomorrow, now));
        std::stringstream res;
        {
          elle::serialization::json::SerializerOut output(res, false);
          output.serialize_forward(creds);
        }
        (*this->_transactions.find(id))->cloud_credentials() = std::move(creds);
        return res.str();
      });

    return elle::UUID(id);
  }

  std::string
  Server::_login_post(Server::Headers const&,
                            Server::Cookies const& cookies,
                            Server::Parameters const&,
                            elle::Buffer const& body)
  {
    elle::IOStream stream(body.istreambuf());
    elle::serialization::json::SerializerIn input(stream, false);
    boost::optional<std::string> email;
    input.serialize("email", email);
    boost::optional<std::string> long_lived_access_token;
    input.serialize("long_lived_access_token", long_lived_access_token);
    bool registered = false;
    auto const& user = [&] () -> User const&
      {
        if (email)
        {
          auto& users = this->_users.get<1>();
          auto it = users.find(email.get());
          if (it == users.end())
            throw reactor::http::tests::Server::Exception(
              "/login",
              reactor::http::StatusCode::Not_Found,
              "user not found");
          return *it;
        }
        else if (long_lived_access_token)
        {
          return this->facebook_connect(long_lived_access_token.get(),
                                        registered);
        }
        elle::unreachable();
      }();
    std::string device_id_str;
    input.serialize("device_id", device_id_str);
    auto device_id = elle::UUID(device_id_str);
    this->headers()["Set-Cookie"] =
      elle::sprintf("session-id=%s+%s", user.id, device_id_str);
    if (this->_devices.find(device_id) == this->_devices.end())
      this->register_device(user, device_id);
    auto const& device = this->_devices.at(device_id);
    std::stringstream res;
    {
      namespace meta_ns = infinit::oracles::meta;
      elle::serialization::json::SerializerOut output(res, false);
      meta_ns::Self self;
      self.connected_devices = std::vector<elle::UUID>{device.id};
      self.devices = std::list<std::string>{device.id.repr()};
      self.email = user.email();
      self.facebook_id = user.facebook_id();
      self.favorites = std::list<std::string>();
      self.fullname = user.fullname;
      self.handle = user.handle;
      self.id = user.id;
      self.public_key = user.public_key;
      self.register_status = user.register_status;
      std::string identity_serialized;
      user.identity()->Save(identity_serialized);
      self.identity = identity_serialized;
      output.serialize("self", self);
      output.serialize("device", static_cast<meta_ns::Device>(device));
      output.serialize("features",
                       std::unordered_map<std::string, std::string>());
      output.serialize(
        "trophonius",
        static_cast<meta_ns::LoginResponse::Trophonius>(*this->trophonius));
      output.serialize("account_registered", registered ? true : false);
    }
    ELLE_DEBUG("login res: %s", res.str());
    return res.str();
  }

  std::string
  Server::_transaction_post(Server::Headers const&,
                            Server::Cookies const& cookies,
                            Server::Parameters const&,
                            elle::Buffer const& content)
  {
    auto const& user = this->user(cookies);
    elle::IOStream stream(content.istreambuf());
    elle::serialization::json::SerializerIn input(stream, false);
    std::string recipient_identifier;
    input.serialize("recipient_identifier", recipient_identifier);
    elle::UUID const& id = this->_create_empty(user.id, recipient_identifier);
    auto const& transaction = **this->_transactions.find(id.repr());
    std::unique_ptr<infinit::oracles::Transaction> ptr(
      new infinit::oracles::PeerTransaction(transaction));
    std::stringstream res;
    {
      elle::serialization::json::SerializerOut output(res, false);
      output.serialize("transaction", ptr);
    }
    return res.str();
  }

  std::string
  Server::_transaction_put(Server::Headers const&,
                           Server::Cookies const& cookies,
                           Server::Parameters const&,
                           elle::Buffer const& content,
                           elle::UUID const& id)
  {
    // FIXME: WTF ??? Just deserialize a transactions FFS ...
    User const& user = this->user(cookies);
    auto const& device = this->device(cookies);
    elle::IOStream stream(content.istreambuf());
    elle::serialization::json::SerializerIn input(stream, false);
    std::string recipient_email_or_id;
    input.serialize("recipient_identifier", recipient_email_or_id);
    ELLE_LOG("%s: recipient: %s", *this, recipient_email_or_id);
    std::list<std::string> files;
    input.serialize("files", files);
    bool ghost = false;
    auto const& rec = [&] () -> User const& {
      if (recipient_email_or_id.find('@') != std::string::npos)
      {
        auto& users_by_email = this->_users.get<1>();
        auto recipient = users_by_email.find(recipient_email_or_id);
        ghost = recipient == users_by_email.end();
        return ghost
        ? generate_ghost_user(recipient_email_or_id)
        : *recipient;
      }
      else
      {
        auto id = recipient_email_or_id;
        auto& users = this->_users.get<0>();
        return *users.find(id);
      }}();

    // BMI shouldn't be used like that...
    auto& tr = **this->_transactions.find(id.repr());
    tr.status = infinit::oracles::Transaction::Status::initialized;
    tr.sender_id = user.id;
    tr.sender_device_id = device.id.repr();
    tr.files = files;
    tr.recipient_id = rec.id;
    tr.is_ghost = ghost;
    int total_size;
    input.serialize("total_size", total_size);
    tr.total_size = total_size;

    struct UpdateSwag
    {
      UpdateSwag(User const& swagger)
        : swagger(swagger)
      {}

      void
      operator()(User& user)
      {
        user.swaggers.insert(&user);
      }

      User const& swagger;
    };
    this->_users.modify(this->_users.get<0>().find(user.id), UpdateSwag(rec));
    this->_users.modify(this->_users.get<0>().find(rec.id), UpdateSwag(user));

    std::stringstream res;
    {
      elle::serialization::json::SerializerOut output(res, false);
      output.serialize("recipient", rec);
      std::unique_ptr<infinit::oracles::Transaction> ptr(
        new infinit::oracles::PeerTransaction(tr));
      output.serialize("transaction", ptr);
    }
    return res.str();
  }

  User const&
  Server::user(Server::Cookies const& cookies) const
  {
    try
    {
      if (contains(cookies, "session-id"))
      {
        std::string session_id = cookies.at("session-id");
        std::vector<std::string> strs;
        boost::split(strs, session_id, boost::is_any_of("+"));
        auto& users = this->_users.get<0>();
        auto it = users.find(strs[0]);
        if (it != users.end())
        {
          return *it;
        }
      }
    }
    catch (...)
    {}
    ELLE_LOG("cookies: %s", cookies);
    for (auto const& user: this->_users.get<0>())
      ELLE_LOG("user: %s", user);
    throw Server::Exception(" ", reactor::http::StatusCode::Forbidden, " ");
  }

  User const&
  Server::user(std::string const& id_or_email) const
  {
    {
      auto& users = this->_users.get<1>();
      auto it = users.find(id_or_email);
      if (it != users.end())
        return *it;
    }
    {
      auto& users = this->_users.get<0>();
      auto it = users.find(id_or_email);
      if (it != users.end())
        return *it;
      throw Server::Exception(
        "", reactor::http::StatusCode::Not_Found, "User not found");
    }
  }

  Device const&
  Server::device(Server::Cookies const& cookies) const
   {
    try
    {
      if (contains(cookies, "session-id"))
      {
        auto& devices = this->_devices;
        std::string session_id = cookies.at("session-id");
        std::vector<std::string> strs;
        boost::split(strs, session_id, boost::is_any_of("+"));
        auto it = devices.find(elle::UUID(strs[1]));
        if (it != devices.end())
        {
          return it->second;
        }
      }
    }
    catch (...)
    {}
    ELLE_LOG("cookies: %s", cookies);
    throw Server::Exception(" ", reactor::http::StatusCode::Forbidden, " ");
  }

  User const&
  Server::register_user(std::string const& email,
                        std::string const& password)
  {
    auto keys =
      infinit::cryptography::rsa::keypair::generate(
        papier::Identity::keypair_length);
    auto password_hash = infinit::oracles::meta::old_password_hash(email, password);
    std::string id(elle::UUID::random().repr());
    ELLE_TRACE_SCOPE("%s: generate user %s", *this, id);
    auto identity =
      generate_identity(keys, boost::lexical_cast<std::string>(id), "my identity", password_hash);
    std::string identity_serialized;
    identity->Save(identity_serialized);
    auto response =
      [this, id, email, identity_serialized, keys]
      (Server::Headers const&,
       Server::Cookies const&,
       Server::Parameters const&,
       elle::Buffer const&)
      {
        auto& users = this->_users.get<0>();
        auto it = users.find(id);
        if (it == users.end())
          throw reactor::http::tests::Server::Exception(
            elle::sprintf("user/%s not found", id),
            reactor::http::StatusCode::Not_Found,
            "user not found");
        return (*it).json();
      };
    this->register_route(elle::sprintf("/users/%s", email),
                         reactor::http::Method::GET, response);
    this->register_route(elle::sprintf("/users/%s", id),
                         reactor::http::Method::GET, response);
    this->_users.emplace(id, email, std::move(keys), std::move(identity));
    {
      auto const& users = this->_users.get<0>();
      auto it = users.find(id);
      ELLE_ASSERT(it != users.end());
      return *it;
    }
  }

  User const&
  Server::generate_ghost_user(std::string const& email)
  {
    ELLE_ASSERT(this->_users.get<1>().find(email) == this->_users.get<1>().end());
    std::string id(elle::UUID::random().repr());

    auto response =
      [this, id, email]
      (Server::Headers const&,
       Server::Cookies const&,
       Server::Parameters const&,
       elle::Buffer const&)
      {
        ELLE_TRACE_SCOPE("%s: fetch ghost user %s (%s)", *this, id, email);
        return elle::sprintf(
          "{"
          "  \"id\": \"%s\","
          "  \"public_key\": \"\","
          "  \"fullname\": \"Eric Draven\","
          "  \"handle\": \"thecrow\","
          "  \"connected_devices\": [],"
          "  \"status\": false,"
          "  \"register_status\": \"ghost\""
          "}",
          id);
      };
    this->register_route(elle::sprintf("/users/%s", email),
                         reactor::http::Method::GET, response);
    this->register_route(elle::sprintf("/users/%s", id),
                         reactor::http::Method::GET, response);
    this->_users.emplace(id,
                         email,
                         boost::optional<infinit::cryptography::rsa::KeyPair>{},
                         std::unique_ptr<papier::Identity>{});

    {
      auto& users = this->_users.get<0>();
      auto it = users.find(id);
      ELLE_ASSERT(it != users.end());
      return *it;
    }
  }

  void
  Server::register_device(User const& user,
                          boost::optional<elle::UUID> device_id)
  {
    Device device{ user.identity()->pair().K(), device_id };
    auto id = user.id;
    this->_users.modify(this->_users.get<0>().find(user.id),
                        [id] (User& user) { user.devices.insert(id); });
    this->_devices.emplace(device.id, device);
    this->register_route(
      elle::sprintf("/device/%s/view", device.id),
      reactor::http::Method::GET,
      [device] (Server::Headers const&,
                Server::Cookies const&,
                Server::Parameters const&,
                elle::Buffer const&)
      {
        return device.json();
      });
  }

  std::string
  Server::_get_trophonius(Headers const&,
                          Cookies const&,
                          Parameters const&,
                          elle::Buffer const&) const
  {
    return this->trophonius->json();
  }

  Transaction&
  Server::transaction(std::string const& id)
  {
    auto it = this->_transactions.find(id);
    ELLE_ASSERT(it != this->_transactions.end());
    return **it;
  }

  void
  Server::session_id(elle::UUID id)
  {
    this->_session_id = std::move(id);
  }

  void
  Server::_maybe_sleep()
  {
  }

  void SleepyServer::_maybe_sleep()
  {
    this->started_blocking.open();
    reactor::sleep(boost::posix_time::minutes(2));
  }
}

std::unique_ptr<papier::Identity>
generate_identity(infinit::cryptography::rsa::KeyPair const& keypair,
                  std::string const& id,
                  std::string const& description,
                  std::string const& password)
{
  std::unique_ptr<papier::Identity> identity(new papier::Identity);
  if (identity->Create(id, description, keypair) == elle::Status::Error)
    throw std::runtime_error("unable to create the identity");
  if (identity->Encrypt(password) == elle::Status::Error)
    throw std::runtime_error("unable to encrypt the identity");
  if (identity->Seal(tests::authority) == elle::Status::Error)
    throw std::runtime_error("unable to seal the identity");
  return identity;
}
