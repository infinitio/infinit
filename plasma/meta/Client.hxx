#pragma once
#ifndef  PLASMA_META_CLIENT_HXX
# define PLASMA_META_CLIENT_HXX

# include "curly.hh"

namespace plasma
{
  namespace meta
  {

    template <class Container>
    NetworkConnectDeviceResponse
    Client::network_connect_device(std::string const& network_id,
                                   std::string const& device_id,
                                   Container const& local_ips)
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
                                   Container2 const& public_endpoints)
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
    curl_debug_callback (CURL *handle,
                         curl_infotype type,
                         char *what,
                         size_t what_size,
                         void *userptr)
    {
      ELLE_LOG_COMPONENT("infinit.plasma.meta.Client.curl");
      std::map<curl_infotype, std::string> symbols = {
        {CURLINFO_TEXT, "*"},
        {CURLINFO_HEADER_IN, "<"},
        {CURLINFO_HEADER_OUT, ">"},
        {CURLINFO_DATA_IN, "<<"},
        {CURLINFO_DATA_OUT, ">>"},
      };
      // get rid of \n
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
      ELLE_DEBUG("%s %s", sym, msg);
      return 0;
    }

    template <typename T>
    T
    Client::_post(std::string const& url, elle::format::json::Object const& req)
    {
      curly::request_configuration c;
      std::stringstream in;
      std::stringstream out;
      
      req.repr(in);
      c.option(CURLOPT_POST, 1);
      c.option(CURLOPT_VERBOSE, 1);
      c.option(CURLOPT_POSTFIELDSIZE, in.str().size());
      c.option(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
      c.option(CURLOPT_DEBUGDATA, nullptr);

      c.url(elle::sprintf("%s%s", this->_root_url, url));
      c.input(in);
      c.output(out);
      c.option(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
      c.headers({
        {"Authorization", this->_token},
        {"User-Agent", this->_user_agent},
        {"Connection", "close"},
      });
      curly::request request(std::move(c));
      return this->_deserialize_answer<T>(out);
    }

    template <typename T>
    T
    Client::_get(std::string const& url)
    {
      std::stringstream resp;
      curly::request_configuration c;

      c.option(CURLOPT_HTTPGET, 1);
      c.option(CURLOPT_VERBOSE, 1);
      c.option(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
      c.option(CURLOPT_DEBUGDATA, nullptr);

      c.url(elle::sprintf("%s%s", this->_root_url, url));
      c.output(resp);
      c.option(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
      c.headers({
        {"Authorization", this->_token},
        {"User-Agent", this->_user_agent},
        {"Connection", "close"},
      });
      curly::request request(std::move(c));
      return this->_deserialize_answer<T>(resp);
    }

    template <typename T>
    T
    Client::_deserialize_answer(std::istream& res)
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
        ELLE_ERR("Couldn't deserialize %s: %s", ELLE_PRETTY_TYPE(T), err.what());
        throw Exception(Error::unknown, err.what());
      }

      if (ret.success() != true)
        throw Exception(ret.error_code, ret.error_details);

      return ret;
    }
  }
}

#endif /* end of include guard:  PLASMA_META_CLIENT_HXX */
