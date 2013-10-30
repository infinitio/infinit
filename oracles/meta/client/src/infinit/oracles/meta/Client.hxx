#ifndef PLASMA_META_CLIENT_HXX
# define PLASMA_META_CLIENT_HXX

# include <infinit/oracles/meta/Client.hh>

# include <elle/serialize/JSONArchive.hh>
# include <elle/serialize/SetSerializer.hxx>

# include <boost/algorithm/string/split.hpp>
# include <boost/algorithm/string/classification.hpp>

# include <elle/Buffer.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      template <typename T>
      T
      Client::_request(std::string const& url,
                       reactor::http::Method method) const
      {
        return this->_request<T>(url, method,
                                 *(elle::format::json::Object*)(nullptr));
      }

      template <typename T>
      T
      Client::_request(std::string const& url,
                       reactor::http::Method method,
                       elle::format::json::Object const& body) const
      {
        ELLE_LOG_COMPONENT("oracles.meta.client");
        ELLE_TRACE_SCOPE("%s: %s on %s", *this, method, url);

        reactor::http::Request request =
          [&]
          {
            if (&body)
            {
              reactor::http::Request request(this->_root_url + url,
                                             method,
                                             "application/json",
                                             this->_default_configuration);

              request << body.repr();
              return std::move(request);
            }
            else
              return std::move(
                reactor::http::Request(this->_root_url + url,
                                       method,
                                       this->_default_configuration));
          }();

        request.finalize();

        auto status = request.status();
        if (status != reactor::http::StatusCode::OK)
          throw elle::http::Exception(
            static_cast<elle::http::ResponseCode>(status),
            elle::sprintf("error %s while posting on %s", status, url));

        // deserialize response
        T ret;
        try
        {
          // FIXME: this intermediate step is because the JSON parser performs
          // crappy seekg, remove as soon as we use a real one.
          std::stringstream input;
          std::copy(std::istream_iterator<char>(request),
                    std::istream_iterator<char>(),
                    std::ostream_iterator<char>(input));
          elle::serialize::InputJSONArchive(input, ret);
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
}

namespace std
{
  template<>
  struct hash<infinit::oracles::meta::User>
  {
  public:
    std::size_t
    operator()(infinit::oracles::meta::User const& user) const
    {
      return std::hash<std::string>()(user.id);
    }
  };
}

#endif
