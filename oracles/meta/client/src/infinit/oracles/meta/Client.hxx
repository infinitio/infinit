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
      Client::_deserialize_answer(reactor::http::Request request) const
      {
        ELLE_LOG_COMPONENT("oracles.meta.client");
        // deserialize response
        T ret;
        try
        {
          // FIXME: this intermediate step is because the JSON parser performs
          // crappy seekg, remove as soon as we use a real one.
          std::stringstream input;
          std::copy(std::istreambuf_iterator<char>(request),
                    std::istreambuf_iterator<char>(),
                    std::ostreambuf_iterator<char>(input));

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
