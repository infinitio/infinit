#ifndef ELLE_FORMAT_INI_SECTION_HXX
# define ELLE_FORMAT_INI_SECTION_HXX

# include <sstream>
# include <stdexcept>

# include <elle/Exception.hh>
# include <elle/printf.hh>

namespace elle { namespace format { namespace ini {

    template<typename T>
      T Section::Get(elle::String const& key) const
      {
        auto it = _pairs.find(key);
        if (it != _pairs.end())
          {
            T value;
            std::stringstream ss(it->second);
            ss >> value;
            if (ss.fail())
              throw std::runtime_error("Could not convert '" + it->second + "'");
            return value;
          }
        throw std::runtime_error("KeyError: '" + key + "'");
      }

    template<typename T>
      T Section::Get(elle::String const& key, T const& default_value) const
      {
        auto it = _pairs.find(key);
        if (it != _pairs.end())
          {
            T value;
            std::stringstream ss(it->second);
            ss >> value;
            if (ss.fail())
              throw elle::Exception(elle::sprintf("Could not convert '%s'",
                                                  it->second));
            return value;
          }
        return default_value;
      }

    template<typename T>
    void Section::Set(elle::String const& key, T const& value)
    {
      std::stringstream ss;
      ss << value;
      _pairs[key] = ss.str();
    }

}}}

#endif
