#include "State.hh"
#include "_detail/Operation.hh"

#include <common/common.hh>

#include <protocol/Serializer.hh>
#include <protocol/ChanneledStream.hh>

#include <etoile/portal/Portal.hh>

#include <reactor/network/tcp-socket.hh>
#include <reactor/sleep.hh>

#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <metrics/_details/google.hh>
#include <metrics/_details/kissmetrics.hh>
#include <elle/memory.hh>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <hole/implementations/slug/Manifest.hh>

#include <fstream>
#include <iterator>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;

    // - Exception ------------------------------------------------------------
    Exception::Exception(gap_Status code, std::string const& msg)
      : std::runtime_error(msg)
      , code(code)
    {}

    // - State ----------------------------------------------------------------
    State::State()
      : _meta{new plasma::meta::Client{
          common::meta::host(), common::meta::port(), true,
        }}
      , _trophonius{nullptr}
      , _reporter{}
      , _google_reporter{}
      , _users{}
      , _logged{false}
      , _swaggers_dirty{true}
      , _output_dir{common::system::download_directory()}
      , _files_infos{}
      , _networks{}
      , _networks_dirty{true}
      , _infinit_instance_manager{}
    {
      std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");
      if (!log_file.empty())
        this->output_log_file(log_file);

      ELLE_LOG("Creating a new State");

      this->transaction_callback(
          [&] (TransactionNotification const &n, bool is_new) -> void {
            this->_on_transaction(n, is_new);
          }
      );
      this->transaction_status_callback(
          [&] (TransactionStatusNotification const &n, bool) -> void {
            this->_on_transaction_status(n);
          }
      );
      this->user_status_callback(
          [&] (UserStatusNotification const &n) -> void {
            this->_on_user_status_update(n);
          }
      );

      std::string user = elle::os::getenv("INFINIT_USER", "");

      if (user.length() > 0)
      {
        std::string identity_path = common::infinit::identity_path(user);

        if (identity_path.length() > 0 && fs::exists(identity_path))
        {
          std::ifstream identity;
          identity.open(identity_path);

          if (!identity.good())
            return;

          std::string token;
          std::getline(identity, token);

          std::string ident;
          std::getline(identity, ident);

          std::string mail;
          std::getline(identity, mail);

          std::string id;
          std::getline(identity, id);

          this->_meta->token(token);
          this->_meta->identity(ident);
          this->_meta->email(mail);

          this->_me = this->_meta->self();
        }
      }

      // Initialize google metrics.
      auto const& g_info = common::metrics::google_info();
      elle::metrics::google::register_service(this->_google_reporter,
                                              g_info.server,
                                              g_info.port,
                                              g_info.id_path);

      // Initialize server.
      auto const& km_info = common::metrics::km_info();
      elle::metrics::kissmetrics::register_service(this->_reporter,
                                                   km_info.server,
                                                   km_info.port,
                                                   km_info.id_path);
    }

    void
    State::output_log_file(std::string const& path)
    {
      static std::ofstream out{
          path + ".log",
          std::fstream::trunc | std::fstream::out
      };

      elle::log::logger(
          std::unique_ptr<elle::log::Logger>{new elle::log::TextLogger(out)}
      );
    }

    State::State(std::string const& token):
      State{}
    {
      ELLE_LOG("Creating a new State with token");
      this->_meta->token(token);
      auto res = this->_meta->self();
      this->_meta->identity(res.identity);
      this->_meta->email(res.email);
      //XXX factorize that shit
      this->_me = res;
    }

    void
    State::on_error_callback(OnErrorCallback const& cb)
    {
      _error_handlers.push_back(cb);
    }

    State::~State()
    {
      ELLE_WARN("Destroying state.");
      this->logout();
    }

    void
    State::debug()
    {
      this->_meta->debug();
    }

    // - TROPHONIUS ----------------------------------------------------
    /// Connect to trophonius
    ///

    auto _print = [] (std::string const &s) { ELLE_DEBUG("-- %s", s); };

    static
    std::vector<std::string>
    _find_commond_addr(std::list<std::string> const &externals,
                       std::list<std::string> const &my_externals)
    {
      std::vector<std::string> theirs_addr;
      std::vector<std::string> ours_addr;
      std::vector<std::string> common_addr;

      // XXX[refactor this]
      for (auto const& i: externals)
      {
        std::vector<std::string> res;
        boost::split(res, i, boost::is_any_of(":"));
        // XXX filter backup
        if (res[1] == "9898")
          continue;
        theirs_addr.push_back(res[0]);
      }
      // XXX[refactor this]
      for (auto const& i: my_externals)
      {
        std::vector<std::string> res;
        boost::split(res, i, boost::is_any_of(":"));
        // XXX filter backup
        if (res[1] == "9898")
          continue;
        ours_addr.push_back(res[0]);
      }

      std::set_intersection(begin(theirs_addr), end(theirs_addr),
                            begin(ours_addr), end(ours_addr),
                            std::back_inserter(common_addr));
      return common_addr;
    }

    static
    int
    _connect_try(reactor::Scheduler& sched,
                 hole::implementations::slug::control::RPC& rpcs,
                 std::vector<std::string> const& addresses)
    {
      std::vector<
        std::pair<
          std::unique_ptr<reactor::VThread<bool>>, std::string
        >
      > v;

      auto slug_connect = [&] (std::string const& endpoint) {
        std::vector<std::string> result;
        boost::split(result, endpoint, boost::is_any_of(":"));

        auto const &ip = result[0];
        auto const &port = result[1];
        ELLE_DEBUG("slug_connect(%s, %s)", ip, port)
          rpcs.slug_connect(ip, std::stoi(port));

        ELLE_DEBUG("slug_wait(%s, %s)", ip, port)
          if (!rpcs.slug_wait(ip, std::stoi(port)))
            throw elle::Exception(elle::sprintf("slug_wait(%s, %s) failed",
                                                ip, port));
      };

      auto start_thread = [&] (std::string const &endpoint) {
        v.push_back(std::make_pair(elle::make_unique<reactor::VThread<bool>>(
          sched,
          elle::sprintf("slug_connect(%s)", endpoint),
          [&] () -> int {
            try {
              slug_connect(endpoint);
            }
            catch (elle::Exception const &e) {
              ELLE_WARN("slug_connect failed: %s", e.what());
              return false;
            }
            return true;
          }
        ), endpoint));
      };

      ELLE_DEBUG("Connecting...")
        std::for_each(std::begin(addresses), std::end(addresses), start_thread);

      int i = 0;
      for (auto &t : v)
      {
        reactor::VThread<bool> &vt = *t.first;
        sched.current()->wait(vt);
        if (vt.result() == true)
        {
          i++;
          ELLE_WARN("connection to %s succeed", t.second);
        }
        else
        {
          ELLE_WARN("connection to %s failed", t.second);
        }
      }
      ELLE_TRACE("finish connecting to %d node%s", i, i > 0 ? "s" : "");
      return i;
    }

    void
    State::_notify_8infinit(Transaction const& trans, reactor::Scheduler& sched)
    {
      ELLE_TRACE("Notify 8infinit for transaction %s", trans);

      namespace proto = infinit::protocol;
      std::string const& network_id = trans.network_id;

      /// Check if network is valid
      {
        auto network = this->networks().find(network_id);

        if (network == this->networks().end())
          throw gap::Exception{gap_internal_error, "Unable to find network"};
      }

      // Fetch Nodes and find the correct one to contact
      std::list<std::string> externals;
      std::list<std::string> locals;
      std::list<std::string> my_externals;
      std::list<std::string> my_locals;
      std::list<std::string> fallback;
      {
        std::string theirs_device;
        std::string ours_device;

        if (trans.recipient_device_id == this->device_id())
        {
          theirs_device = trans.sender_device_id;
          ours_device = trans.recipient_device_id;
        }
        else
        {
          theirs_device = trans.recipient_device_id;
          ours_device = trans.sender_device_id;
        }

        // theirs
        {
          Endpoint e = this->_meta->device_endpoints(network_id,
                                                     ours_device,
                                                     theirs_device);

          externals = std::move(e.externals);
          locals = std::move(e.locals);
        }
        //ours
        {
          Endpoint e = this->_meta->device_endpoints(network_id,
                                                     theirs_device,
                                                     ours_device);

          my_externals = std::move(e.externals);
          my_locals = std::move(e.locals);
        }
        // fallback
        {
          // the only exact match ip:port is the fallback server.
          std::set_intersection(begin(my_externals), end(my_externals),
                                begin(externals), end(externals),
                                std::back_inserter(fallback));
          for (auto const& s: fallback)
          {
            externals.remove(s);
            my_externals.remove(s);
          }
        }
      }

      ELLE_DEBUG("externals")
        std::for_each(begin(externals), end(externals), _print);
      ELLE_DEBUG("locals")
        std::for_each(begin(locals), end(locals), _print);
      ELLE_DEBUG("fallback")
        std::for_each(begin(fallback), end(fallback), _print);

      // Very sophisticated heuristic to deduce the addresses to try first.
      std::vector<std::vector<std::string>> rounds;
      {
        std::vector<std::string> common = _find_commond_addr(externals,
                                                             my_externals);
        std::vector<std::string> first_round;
        std::vector<std::string> second_round;

        // sort the list, in order to have a deterministic behavior
        externals.sort();
        locals.sort();
        fallback.sort();

        if (externals.empty() || my_externals.empty())
        {
          for (auto const& s: locals)
            first_round.push_back(s);
          rounds.push_back(first_round);
          for (auto const& s: fallback)
            second_round.push_back(s);
          rounds.push_back(second_round);
        }
        else if (common.empty())
        {
          // if there is no common external address, then we can try them first.
          for (auto const& s: externals)
            first_round.push_back(s);
          rounds.push_back(first_round);
          // then, we know we can not connect locally, so try to fallback
          for (auto const& s: fallback)
            second_round.push_back(s);
          rounds.push_back(second_round);
        }
        else
        {
          // if there is a common external address, we can try to connect to
          // local endpoints
          std::vector<std::string> addr = _find_commond_addr(locals,
                                                             my_locals);

          if (!addr.empty())
          {
            // wtf, you are trying to do a local exchange, this is stupid, but
            // let it be.
            first_round.push_back(locals.front());
            rounds.push_back(first_round);
            if (addr.size() > 1)
            {
              second_round.push_back(locals.back());
              rounds.push_back(second_round);
            }
          }
          else
          {
            std::vector<std::string> third_round;
            // try local first
            for (auto const &s: locals)
              first_round.push_back(s);
            rounds.push_back(first_round);
            // then externals
            for (auto const& s: externals)
              second_round.push_back(s);
            rounds.push_back(second_round);
            // then fallback
            for (auto const& s: fallback)
              third_round.push_back(s);
            rounds.push_back(third_round);
          }
        }
      }

      // Finish by calling the RPC to notify 8infinit of all the IPs of the peer
      {
        lune::Phrase phrase;

        if (this->_wait_portal(network_id) == false)
          throw Exception{gap_error, "Couldn't find portal to infinit instance"};

        phrase.load(this->_me._id, network_id, "slug");

        ELLE_DEBUG("Connect to the local 8infint instance (%s:%d)",
                   elle::String{"127.0.0.1"},
                   phrase.port);

        // Connect to the server.
        reactor::network::TCPSocket socket{
          sched,
          elle::String("127.0.0.1"),
          phrase.port,
        };

        proto::Serializer serializer{
          sched,
          socket
        };

        proto::ChanneledStream channels{
          sched,
          serializer
        };

        hole::implementations::slug::control::RPC rpcs{channels};

        int i = 1;
        ELLE_DEBUG("DEBUG ROUNDS")
          for (auto const& round: rounds)
          {
            ELLE_DEBUG("- ROUND %s", i++)
              for (auto const& addr: round)
              {
                ELLE_DEBUG("-- %s", addr);
              }
          }
        int round_number = 1;
        bool success = false;
        for (auto const& round: rounds)
        {
          ELLE_DEBUG("ROUND %s:", round_number++)
            for (auto const&s : round)
              ELLE_DEBUG("-- %s", s);
          if (_connect_try(sched, rpcs, round) > 0)
          {
            success = true;
            break;
          }
        }
        if (!success)
          throw elle::Exception{"Unable to connect"};
      }
    }
  }
}
