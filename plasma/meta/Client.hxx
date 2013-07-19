#ifndef PLASMA_META_CLIENT_HXX
# define PLASMA_META_CLIENT_HXX

# include <curly/curly.hh>
# include <plasma/meta/Client.hh>

# include <elle/serialize/JSONArchive.hh>

# include <boost/algorithm/string/split.hpp>
# include <boost/algorithm/string/classification.hpp>

namespace plasma
{
  namespace meta
  {

    template <class Container>
    NetworkConnectDeviceResponse
    Client::network_connect_device(std::string const& network_id,
                                   std::string const& device_id,
                                   Container const& local_ips) const
    {
      adapter_type local_adapter;
      adapter_type public_adapter;

      for (auto &a: local_ips)
      {
        local_adapter.emplace_back(a.first, a.second);
      }

      return this->_network_connect_device(network_id,
                                           device_id,
                                           local_adapter,
                                           public_adapter);
    }

    template <class Container1, class Container2>
    NetworkConnectDeviceResponse
    Client::network_connect_device(std::string const& network_id,
                                   std::string const& device_id,
                                   Container1 const& local_ips,
                                   Container2 const& public_endpoints) const
    {
      adapter_type local_adapter;
      adapter_type public_adapter;

      for (auto &a: local_ips)
      {
        local_adapter.emplace_back(a.first, a.second);
      }

      for (auto &a: public_endpoints)
      {
        public_adapter.emplace_back(a.first, a.second);
      }

      return this->_network_connect_device(network_id,
                                           device_id,
                                           local_adapter,
                                           public_adapter);
    }

    static inline
    int
    curl_debug_callback(CURL* handle,
                        curl_infotype type,
                        char* what,
                        size_t what_size,
                        void* userptr)
    {
      ELLE_LOG_COMPONENT("infinit.plasma.meta.Client.curl");

      Client* client = reinterpret_cast<Client*>(userptr);

      (void)handle;
      (void)userptr;
      std::map<curl_infotype, std::string> symbols = {
        {CURLINFO_TEXT, "*"},
        {CURLINFO_HEADER_IN, "<"},
        {CURLINFO_HEADER_OUT, ">"},
        {CURLINFO_DATA_IN, "<<"},
        {CURLINFO_DATA_OUT, ">>"},
      };
      // get rid of \n
      ELLE_ASSERT_GT(what_size, 0u);
      if (what[what_size - 1] == '\n')
      {
        what_size--;
        what[what_size] = '\0';
      }

      std::string msg{what, what + what_size};
      std::string sym;
      try
      {
        sym = symbols.at(type);
      }
      catch (std::out_of_range const&)
      {
        sym = "*";
      }

      if (type == CURLINFO_TEXT)
      {
        ELLE_DUMP("%s: %s %s", *client, sym, msg);
      }
      else if (type == CURLINFO_HEADER_OUT)
      {
        std::vector<std::string> v;

        boost::split(v, msg, boost::algorithm::is_any_of("\n"));
        if (v.size() > 0)
        {
          ELLE_TRACE_SCOPE("%s: %s %s", *client, sym, v[0]);
          int i = 0;
          for (auto const&s : v)
          {
            if (i++ == 0)
              continue;
            ELLE_DEBUG("%s: %s %s", *client, sym, s);
          }
        }
      }
      else if (type == CURLINFO_DATA_IN || type == CURLINFO_DATA_OUT)
      {
        ELLE_TRACE("%s: %s %s", *client, sym, msg);
      }
      else if (type == CURLINFO_HEADER_IN)
      {
        if (msg.find("HTTP") == 0) // starts with
          ELLE_TRACE("%s: %s %s", *client, sym, msg);
        else
          ELLE_DUMP("%s: %s %s", *client, sym, msg);
      }
      return 0;
    }

    template <typename T>
    T
    Client::_post(std::string const& url,
                  elle::format::json::Object const& req) const
    {
      // XXX Curl is supposed to be thread-safe.
      std::unique_lock<std::mutex> lock(this->_mutex);
      std::stringstream in;
      std::stringstream out;
      curly::request_configuration c = curly::make_post();

      c.option(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
      c.option(CURLOPT_DEBUGDATA, this);

      req.repr(in);
      c.option(CURLOPT_POSTFIELDSIZE, in.str().size());
      c.option(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
      c.url(elle::sprintf("%s%s", this->_root_url, url));
      c.user_agent(this->_user_agent);
      c.input(in);
      c.output(out);
      c.headers({
        {"Authorization", this->_token},
        {"Connection", "close"},
      });
      curly::request request(std::move(c));
      return this->_deserialize_answer<T>(out);
    }

    template <typename T>
    T
    Client::_get(std::string const& url) const
    {
      // XXX Curl is supposed to be thread-safe.
      std::unique_lock<std::mutex> lock(this->_mutex);
      std::stringstream resp;
      curly::request_configuration c = curly::make_get();

      c.option(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
      c.option(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
      c.option(CURLOPT_DEBUGDATA, this);

      c.url(elle::sprintf("%s%s", this->_root_url, url));
      c.output(resp);
      c.user_agent(this->_user_agent);
      c.headers({
        {"Authorization", this->_token},
        {"Connection", "close"},
      });
      curly::request request(std::move(c));
      return this->_deserialize_answer<T>(resp);
    }

    template <typename T>
    T
    Client::_deserialize_answer(std::istream& res) const
    {
      T ret;
      ELLE_LOG_COMPONENT("infinit.plasma.meta.Client");
      // deserialize response
      try
      {
        elle::serialize::InputJSONArchive(res, ret);
      }
      catch (std::exception const& err)
      {
        ELLE_ERR("%s: Couldn't deserialize %s: %s",
                 *this,
                 ELLE_PRETTY_TYPE(T),
                 err.what());
        throw Exception(Error::unknown, err.what());
      }

      if (ret.success() != true)
        throw Exception(ret.error_code, ret.error_details);

      return ret;
    }
  }
}

#endif
