#include "server.hh"

#include <boost/uuid/random_generator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <elle/Buffer.hh>
#include <elle/log.hh>
#include <elle/finally.hh>
#include <elle/container/map.hh>
#include <elle/serialization/json/SerializerIn.hh>
#include <elle/serialization/json.hh>

#include <papier/Authority.hh>

#include <version.hh>

#include <fist/tests/_detail/Authority.hh>

ELLE_LOG_COMPONENT("fist.tests");


std::unique_ptr<papier::Identity>
generate_identity(cryptography::KeyPair const& keypair,
                  std::string const& id,
                  std::string const& description,
                  std::string const& password);

template <typename C>
std::string
json(C& container)
{
  std::string str = "[";
  for (auto& item: container)
    str += item->json() + ", ";

  if (!container.empty())
    str = str.substr(0, str.length() - 2);
  str += "]";
  return str;
}

namespace tests
{
  Server::Server()
    : _session_id(random_uuid())
    , trophonius()
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
      [&] (Server::Headers const&,
           Server::Cookies const&,
           Server::Parameters const&,
           elle::Buffer const& content)
      {
        elle::IOStream stream(content.istreambuf());
        elle::serialization::json::SerializerIn input(stream, false);
        std::string email;
        input.serialize("email", email);
        std::string device_id_str;
        input.serialize("device_id", device_id_str);
        auto device_id = boost::uuids::string_generator()(device_id_str);
        auto& users = this->_users.get<1>();
        auto it = users.find(email);
        if (it == users.end())
          throw reactor::http::tests::Server::Exception(
            "/login",
            reactor::http::StatusCode::Not_Found,
            "user not found");
        auto& user = **it;
        this->headers()["Set-Cookie"] = elle::sprintf("session-id=%s+%s", user.id(), device_id_str);
        if (this->_devices.find(device_id) == this->_devices.end())
          this->register_device(user, device_id);
        auto& device = this->_devices.at(device_id);
        return elle::sprintf(
          "{"
          " \"self\": %s,"
          " \"device\": %s,"
          " \"features\": [],"
          " \"trophonius\" : %s"
          "}",
          user.self_json(),
          device.json(),
          this->trophonius.json());
      });

    // this->register_route(
    //   "/users",
    //   reactor::http::Method::GET,
    //   [&] (Server::Headers const&,
    //        Server::Cookies const& cookies,
    //        Server::Parameters const& parameters,
    //        elle::Buffer const&)
    //   {

    //   });
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
        auto& user = this->user(cookies);
        std::vector<Transaction*> runnings;
        std::vector<Transaction*> finals;
        for (auto& transaction: this->_transactions)
          if (transaction->sender_id == boost::lexical_cast<std::string>(user.id()) ||
              transaction->recipient_id == boost::lexical_cast<std::string>(user.id()))
          {
            auto const& final_statuses = transaction->sender_id == boost::lexical_cast<std::string>(user.id())
              ? surface::gap::Transaction::sender_final_statuses
              : surface::gap::Transaction::recipient_final_statuses;

            if (final_statuses.find(transaction->status) == final_statuses.end())
              runnings.emplace_back(transaction.get());
            else
              finals.emplace_back(transaction.get());
          }
        auto res = elle::sprintf(
          "{"
          "  \"swaggers\": %s,"
          "  \"running_transactions\": %s,"
          "  \"final_transactions\": %s,"
          "  \"links\": %s"
          "}",
          user.swaggers_json(),
          json(runnings),
          json(finals),
          user.links_json());
        return res;
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
      "/facebook_connect",
      reactor::http::Method::POST,
      [&] (Server::Headers const&,
           Server::Cookies const&,
           Server::Parameters const&,
           elle::Buffer const& content)
      {
        elle::IOStream stream(content.istreambuf());
        elle::serialization::json::SerializerIn input(stream, false);
        std::string code;
        input.serialize("code", code);
        std::string device_id_str;
        input.serialize("device_id", device_id_str);
        auto device_id = boost::uuids::string_generator()(device_id_str);

        static std::unordered_map<std::string, std::string> facebook_ids;
        if (facebook_ids.find(code) == facebook_ids.end())
        {
          auto& user = this->register_user("", "");
          // Replace identity.
          auto keys =
            cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                            papier::Identity::keypair_length);
          std::unique_ptr<papier::Identity> identity{generate_identity(keys, boost::lexical_cast<std::string>(user.id()), "my identity", "")};
          user.identity().reset(identity.release());
          facebook_ids[code] = user.facebook_id();
        }
        auto const& facebook_id = facebook_ids[code];
        auto& users = this->_users.get<2>();
        auto it = users.find(facebook_id);
        if (it == users.end())
          throw reactor::http::tests::Server::Exception(
            "/facebook_connect",
            reactor::http::StatusCode::Not_Found,
            "user not found");
        auto& user = **it;
        this->headers()["Set-Cookie"] = elle::sprintf("session-id=%s+%s", user.id(), device_id_str);
        if (this->_devices.find(device_id) == this->_devices.end())
          this->register_device(user, device_id);
        auto& device = this->_devices.at(device_id);
        return elle::sprintf(
          "{"
          " \"self\": %s,"
          " \"device\": %s,"
          " \"features\": [],"
          " \"trophonius\" : %s"
          "}",
          user.self_json(),
          device.json(),
          this->trophonius.json());
      });

    this->register_route(
      "/user/self",
      reactor::http::Method::GET,
      [&] (Server::Headers const& header,
           Server::Cookies const& cookies,
           Server::Parameters const& parameters,
           elle::Buffer const& buffer)
      {
        auto id = this->user(cookies).id();
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
        auto& user = this->user(cookies);
        infinit::oracles::LinkTransaction t;
        t.id = boost::lexical_cast<std::string>(random_uuid());
        auto id = t.id;
        user.links[id] = t;

      this->register_route(
        elle::sprintf("/link/%s", id),
        reactor::http::Method::PUT,
        [&, id] (Server::Headers const&,
             Server::Cookies const& cookies,
             Server::Parameters const& parameters,
             elle::Buffer const& body)
        {
          auto& user = this->user(cookies);
          auto& t =  user.links[id];
          auto device = this->device(cookies);
          t.click_count = 3;
          t.ctime = 2173213;
          t.sender_id = boost::lexical_cast<std::string>(user.id());
          t.sender_device_id = boost::lexical_cast<std::string>(device.id());
          t.status = infinit::oracles::Transaction::Status::initialized;

          auto res = elle::sprintf(
            "{"
            "  \"transaction\": %s,"
            "  \"aws_credentials\": "
            "  {"
            "    \"protocol\": \"aws\","
            "    \"access_key_id\": \"\","
            "    \"bucket\": \"\","
            "    \"expiration\": \"2016-01-12T09-37-42Z\","
            "    \"folder\": \"%s\","
            "    \"protocol\": \"aws\","
            "    \"region\": \"us-east-1\","
            "    \"secret_access_key\": \"\","
            "    \"session_token\": \"\","
            "    \"current_time\": \"2015-01-12T09-37-42Z\""
            "  }"
            "}",
            User::link_representation(t),
            id);
          return res;
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
            auto& user = this->user(cookies);
            auto& t =  user.links[id];
            input.serialize("status", status);
            t.status =
              static_cast<infinit::oracles::Transaction::Status>(status);
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
            this->headers()["ETag"] = boost::lexical_cast<std::string>(random_uuid());
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
      "/transaction/create_empty",
      reactor::http::Method::POST,
      [&] (Server::Headers const&,
           Server::Cookies const&,
           Server::Parameters const&,
           elle::Buffer const&)
      {
        auto t = elle::make_unique<Transaction>();
        std::string id = t->id;
        ELLE_LOG_SCOPE("%s: create transaction %s", *this, id);
        this->_transactions.insert(std::move(t));
        this->register_route(
          elle::sprintf("/transaction/%s", id),
          reactor::http::Method::PUT,
          [&, id] (Server::Headers const&,
                   Server::Cookies const& cookies,
                   Server::Parameters const&,
                   elle::Buffer const& content)
          {
            auto& user = this->user(cookies);
            auto device = this->device(cookies);
            elle::IOStream stream(content.istreambuf());
            elle::serialization::json::SerializerIn input(stream, false);
            std::string recipient_email_or_id;
            input.serialize("id_or_email", recipient_email_or_id);
            ELLE_DEBUG("%s: recipient: %s", *this, recipient_email_or_id);
            std::list<std::string> files;
            input.serialize("files", files);
            bool ghost = false;
            auto& rec = [&] () -> User& {
              if (recipient_email_or_id.find('@') != std::string::npos)
              {
                auto& users_by_email = this->_users.get<1>();
                auto recipient = users_by_email.find(recipient_email_or_id);
                ghost = recipient == users_by_email.end();
                return ghost
                ? generate_ghost_user(recipient_email_or_id)
                : **recipient;
              }
              else
              {
                auto id = recipient_email_or_id;
                auto& users = this->_users.get<0>();
                return **users.find(boost::uuids::string_generator()(id));
              }}();

            auto& tr = **this->_transactions.find(id);
            tr.status = infinit::oracles::Transaction::Status::initialized;
            tr.sender_id = boost::lexical_cast<std::string>(user.id());
            tr.sender_device_id = boost::lexical_cast<std::string>(device.id());
            tr.files = files;
            tr.recipient_id = boost::lexical_cast<std::string>(rec.id());
            tr.is_ghost = ghost;

            rec.swaggers.insert(&user);
            user.swaggers.insert(&rec);
            auto res = elle::sprintf(
              "{"
              "\"created_transaction_id\": \"%s\","
              "\"recipient\": %s,"
              "\"recipient_is_ghost\": %s"
              "}", id, rec.json(),
              rec.ghost() ? "true" : "false");
            return res;
          });
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
            auto device = this->device(cookies);
            auto& user = this->user(cookies);
            // typedef std::vector<std::pair<std::string, int>> Locals;
            // Locals locals;

            // XXX: Doesn't work.
            elle::IOStream stream(content.istreambuf());
            elle::serialization::json::SerializerIn input(stream, false);
            int port;

            static std::unordered_map<std::string, uint16_t> first_time;
            if (first_time.find(id) == first_time.end())
              first_time[id] = port;
            else
            {
              auto& tr = **this->_transactions.find(id);
              bool sender = tr.sender_device_id == boost::lexical_cast<std::string>(device.id()) &&
                tr.sender_id == boost::lexical_cast<std::string>(user.id());
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
              auto generator = boost::uuids::string_generator();
              auto* to_sender = this->trophonius.socket(generator(tr.sender_id), generator(tr.sender_device_id));
              auto* to_recipient = this->trophonius.socket(generator(tr.recipient_id), generator(tr.recipient_device_id));
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
          elle::sprintf("/transaction/%s/cloud_buffer", id),
          reactor::http::Method::GET,
          [&, id] (Server::Headers const&,
               Server::Cookies const&,
               Server::Parameters const&,
               elle::Buffer const&)
          {
            auto now = boost::posix_time::second_clock::universal_time();
            auto tomorrow = now + boost::posix_time::hours(24);
            return elle::sprintf(
              "{"
              "  \"protocol\": \"aws\","
              "  \"access_key_id\": \"\","
              "  \"secret_access_key\": \"\","
              "  \"session_token\": \"\","
              "  \"region\": \"region\","
              "  \"bucket\": \"bucket\","
              "  \"folder\": \"folder\","
              "  \"expiration\": \"%s\","
              "  \"current_time\": \"%s\""
              "}",
              boost::posix_time::to_iso_extended_string(tomorrow),
              boost::posix_time::to_iso_extended_string(now));
          });

    this->register_route(
      "/transaction/update",
      reactor::http::Method::POST,
      [&] (Server::Headers const&,
           Server::Cookies const& cookies,
           Server::Parameters const&,
           elle::Buffer const& content)
      {
        elle::IOStream stream(content.istreambuf());
        elle::serialization::json::SerializerIn input(stream, false);
        std::string id;
        int status;
        input.serialize("transaction_id", id);
        input.serialize("status", status);
        auto it = this->_transactions.find(id);
        if (it == this->_transactions.end())
        {
          for (auto const& tr: this->_transactions)
            ELLE_WARN("%s", *tr);
          throw reactor::http::tests::Server::Exception(
            "/transaction/update",
            reactor::http::StatusCode::Not_Found,
            "transaction not found");
        }
        auto& tr = **it;
        tr.status = infinit::oracles::Transaction::Status(status);
        tr.status_changed()(tr.status);
        if (tr.status == infinit::oracles::Transaction::Status::accepted)
        {
          auto const& user = this->user(cookies);
          auto device = this->device(cookies);
          tr.recipient_id = boost::lexical_cast<std::string>(user.id());
          tr.recipient_device_id = boost::lexical_cast<std::string>(device.id());
        }
        std::string transaction_notification = tr.json();
        transaction_notification.insert(1, "\"notification_type\":7,");
        for (auto& socket: this->trophonius.clients(boost::uuids::string_generator()(tr.sender_id)))
          socket->write(transaction_notification);
        for (auto& socket: this->trophonius.clients(boost::uuids::string_generator()(tr.recipient_id)))
          socket->write(transaction_notification);
        auto res = elle::sprintf(
          "{"
          "%s"
          "  \"updated_transaction_id\": \"%s\""
          "}",
          tr.status == infinit::oracles::Transaction::Status::accepted
          ? elle::sprintf("  \"recipent_device_id\": \"%s\", \"recipient_device_name\": \"bite\", ", tr.recipient_device_id)
          : std::string{},
          tr.id
        );
        return res;
      });

        auto res = elle::sprintf(
          "{"
          "\"created_transaction_id\": \"%s\""
          "}",
          id);
        return res;
      });

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
      [] (Server::Headers const&,
          Server::Cookies const&,
          Server::Parameters const&,
          elle::Buffer const&)
      {
        return "";
      });
    this->register_route(
      "/s3/folder/meta_data",
      reactor::http::Method::GET,
      [] (Server::Headers const&,
          Server::Cookies const&,
          Server::Parameters const&,
          elle::Buffer const&)
      {
        throw reactor::http::tests::Server::Exception(
          "/s3/folder/meta_data",
          reactor::http::StatusCode::Not_Found,
          "no meta data");
        return "{}";
      });
    this->register_route(
      "/s3/folder/000000000000_0",
      reactor::http::Method::PUT,
      [this] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const&)
      {
        this->_cloud_buffered = true;
        return "";
      });
    this->register_route(
      "/s3/folder/000000000000_0",
      reactor::http::Method::GET,
      [this] (Server::Headers const&,
              Server::Cookies const&,
              Server::Parameters const&,
              elle::Buffer const&)
      {
        this->_cloud_buffered = true;
        return "";
      });
  }

  User&
  Server::user(Server::Cookies const& cookies) const
  {
    try
    {
      if (contains(cookies, "session-id"))
      {
        auto& users = this->_users.get<0>();
        std::string session_id = cookies.at("session-id");
        std::vector<std::string> strs;
        boost::split(strs, session_id, boost::is_any_of("+"));
        auto const& user = users.find(boost::uuids::string_generator()(strs[0]));
        if (user != users.end())
        {
          if (*user != nullptr)
            return **user;
        }
      }
    }
    catch (...)
    {}
    ELLE_LOG("cookies: %s", cookies);
    for (auto const& user: this->_users.get<0>())
      ELLE_LOG("user: %s", *user);
    throw Server::Exception(" ", reactor::http::StatusCode::Forbidden, " ");
  }

  Device
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
        auto const& device = devices.find(boost::uuids::string_generator()(strs[1]));
        if (device != devices.end())
        {
          return device->second;
        }
      }
    }
    catch (...)
    {}
    ELLE_LOG("cookies: %s", cookies);
    throw Server::Exception(" ", reactor::http::StatusCode::Forbidden, " ");
  }

  User&
  Server::register_user(std::string const& email,
                        std::string const& password)
  {
    auto keys =
      cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                      papier::Identity::keypair_length);
    auto password_hash = infinit::oracles::meta::old_password_hash(email, password);
    boost::uuids::uuid id = random_uuid();
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
        return (*it)->json();
      };
    this->register_route(elle::sprintf("/users/%s", email),
                         reactor::http::Method::GET, response);
    this->register_route(elle::sprintf("/users/%s", id),
                         reactor::http::Method::GET, response);
    auto user = elle::make_unique<User>(
      id,
      std::move(email),
      std::move(keys),
      std::move(identity));
    this->_users.insert(std::move(user));

    {
      auto& users = this->_users.get<0>();
      auto it = users.find(id);
      return **it;
    }
  }

  User&
  Server::generate_ghost_user(std::string const& email)
  {
    ELLE_ASSERT(this->_users.get<1>().find(email) == this->_users.get<1>().end());
    auto id = random_uuid();

    auto user = elle::make_unique<User>(
      id,
      std::move(email),
      boost::optional<cryptography::KeyPair>{},
      std::unique_ptr<papier::Identity>{});

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

    auto raw = user.get();
    this->_users.insert(std::move(user));
    return *raw;
  }

  void
  Server::register_device(User& user,
                          boost::optional<boost::uuids::uuid> device_id)
  {
    Device device{ user.identity()->pair().K(), device_id };
    user.devices.insert(device.id());
    this->_devices.emplace(device.id(), device);
    this->register_route(
      elle::sprintf("/device/%s/view", device.id()),
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
    return elle::sprintf(
      "{"
      "  \"host\": \"127.0.0.1\","
      "  \"port\": 0,"
      "  \"port_ssl\": %s"
      "}",
      this->trophonius.port());
  }

  Transaction&
  Server::transaction(std::string const& id)
  {
    auto it = this->_transactions.find(id);
    ELLE_ASSERT(it != this->_transactions.end());
    return **it;
  }

  void
  Server::session_id(boost::uuids::uuid id)
  {
    this->_session_id = std::move(id);
  }
}

std::unique_ptr<papier::Identity>
generate_identity(cryptography::KeyPair const& keypair,
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
