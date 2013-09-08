#ifndef PLASMA_META_CLIENT_HXX
# define PLASMA_META_CLIENT_HXX

# include <curly/curly.hh>
# include <plasma/meta/Client.hh>

# include <elle/serialize/JSONArchive.hh>
# include <elle/serialize/SetSerializer.hxx>
# include <boost/algorithm/string/split.hpp>
# include <boost/algorithm/string/classification.hpp>

namespace plasma
{
  namespace meta
  {
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
      if (what_size == 0)
      {
        ELLE_DEBUG("debug callback is empty");
        return 0;
      }

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
      std::stringstream resp;
      this->_post(url, req, resp);
      return this->_deserialize_answer<T>(resp);
    }

    template <typename T>
    T
    Client::_get(std::string const& url) const
    {
      std::stringstream resp;
      this->_get(url, resp);
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

namespace std
{
  template<>
  struct hash<plasma::meta::Network>
  {
  public:
    std::size_t
    operator()(plasma::meta::Network const& network) const
    {
      return std::hash<std::string>()(network._id);
    }
  };

  template<>
  struct hash<plasma::meta::User>
  {
  public:
    std::size_t
    operator()(plasma::meta::User const& user) const
    {
      return std::hash<std::string>()(user.id);
    }
  };
}

#endif
