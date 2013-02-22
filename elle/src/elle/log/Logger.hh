#ifndef  ELLE_LOG_LOGGER_HH
# define ELLE_LOG_LOGGER_HH

# include <memory>
# include <string>
# include <unordered_map>
# include <vector>

# include <boost/noncopyable.hpp>

# include <elle/attribute.hh>

# include <reactor/storage.hh>

namespace elle
{
  namespace log
  {
    class Indentation
    {
    public:
      virtual
      unsigned int
      indentation() = 0;
      virtual
      void
      indent() = 0;
      virtual
      void
      unindent() = 0;
    };

    template <typename I>
    class RegisterIndenter
    {
    public:
      RegisterIndenter();
    };

    class Logger
      : private boost::noncopyable
    {
    /*------.
    | Level |
    `------*/
    public:
      enum class Level
      {
        log = 0,
        trace,
        debug,
        dump,
      };

    /*-----.
    | Type |
    `-----*/
    public:
      enum class Type
      {
        info,
        warning,
        error,
      };

    /*-------------.
    | Construction |
    `-------------*/
    public:
      Logger();
      virtual
      ~Logger();

    /*------------.
    | Indentation |
    `------------*/
    public:
      unsigned int
      indentation();
      void
      indent();
      void
      unindent();
    private:
      boost::mutex _indentation_mutex;
      std::unique_ptr<Indentation> _indentation;
      template <typename I>
      friend class RegisterIndenter;
      static std::function<std::unique_ptr<Indentation> ()>& _factory();

    /*----------.
    | Messaging |
    `----------*/
    public:
      void message(Level level, elle::log::Logger::Type type, std::string const& values);
      void log(std::string const& msg);
      void trace(std::string const& msg);
      void debug(std::string const& msg);
      void dump(std::string const& msg);
    protected:
      virtual
      void
      _message(Level level,
               elle::log::Logger::Type type,
               std::string const& message) = 0;

    /*-----------.
    | Components |
    `-----------*/
    public:
      bool
      component_enabled(std::string const& name);
    private:
      std::vector<std::string> _component_patterns;
      std::unordered_map<std::string, bool> _component_enabled;
      ELLE_ATTRIBUTE_R(unsigned int, component_max_size);
    };
  }
}

# include <elle/log/Logger.hxx>

#endif
