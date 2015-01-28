#include <iosfwd>

#include <boost/program_options.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <elle/Error.hh>
#include <elle/log.hh>
#include <elle/serialization/json.hh>
#include <elle/utility/Utility.hh>

#include <reactor/scheduler.hh>

#include <oracles/meta/client/src/infinit/oracles/meta/Client.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("oracles.meta.client");

using infinit::oracles::meta::Client;

template <typename T>
struct Convert
{
  static
  T
  convert(std::string const& repr)
  {
    return boost::lexical_cast<T>(repr);
  }
};

template <>
struct Convert<std::string>
{
  static
  std::string
  convert(std::string str)
  {
    return std::move(str);
  }
};


template <>
struct Convert<boost::uuids::uuid>
{
  static
  boost::uuids::uuid
  convert(std::string str)
  {
    try
    {
      boost::uuids::string_generator gen;
      return gen(str);
    }
    catch (std::runtime_error const&)
    {
      throw elle::Error(elle::sprintf("invalid uuid: %s", str));
    }
  }
};

template <typename R, typename ... Args>
struct Invoke
{
  template <typename M, typename ... Passed>
  static
  void
  invoke(Client& client,
         M m,
         std::vector<std::string> const& args,
         Passed&& ... passed)
  {
    if (!args.empty())
      throw elle::Error("too many arguments");
    std::cout << (client.*m)(std::forward<Passed>(passed)...) << std::endl;
  }
};

template <typename ... Args>
struct Invoke<void, Args ...>
{
  template <typename M, typename ... Passed>
  static
  void
  invoke(Client& client,
         M m,
         std::vector<std::string> const& args,
         Passed&& ... passed)
  {
    if (!args.empty())
      throw elle::Error("too many arguments");
    (client.*m)(std::forward<Passed>(passed)...);
  }
};

template <typename R, typename Head, typename ... Tail>
struct Invoke<R, Head, Tail ...>
{
  template <typename M, typename ... Passed>
  static
  void
  invoke(Client& client,
         M m,
         std::vector<std::string> const& args,
         Passed&& ... passed)
  {
    ELLE_DEBUG("decode argument of type %s: %s",
               elle::demangle(typeid(Head).name()), args[0]);
    if (args.empty())
      throw elle::Error("missing argument");
    // FIXME: use iterators instead of copying vector chunks
    std::vector<std::string> next(args.begin() + 1, args.end());
    typedef typename std::remove_const<
      typename std::remove_reference<Head>::type>::type Naked;
    Invoke<R, Tail...>::invoke(
      client, m, next,
      std::forward<Passed>(passed)...,
      Convert<Naked>::convert(std::move(args[0])));
  }
};

static
std::ostream&
operator << (std::ostream& s,
             infinit::oracles::meta::LoginResponse const& r)
{
  elle::serialization::json::SerializerOut output(s, false);
  output.serialize_forward(r);
  return s;
}

static
std::ostream&
operator << (std::ostream& s,
             infinit::oracles::meta::LoginResponse::Trophonius const& r)
{
  elle::serialization::json::SerializerOut output(s, false);
  output.serialize_forward(r);
  return s;
}

template <typename R, typename ... Args>
static
void
register_method(boost::program_options::options_description& options,
                std::string name,
                R (Client::*m)(Args ...) const,
                std::vector<std::function<void (Client&)>>& actions)
{
  options.add_options()(
    name.c_str(),
    boost::program_options::value<std::vector<std::string>>()->multitoken()
    ->notifier(
      [m, name, &actions] (std::vector<std::string> args)
      {
        ELLE_TRACE_SCOPE("enqueue %s(%s)", name, args);
        actions.push_back(
          [name, args, m] (Client& c)
          {
            ELLE_TRACE_SCOPE("invoke %s(%s)", name, args);
            Invoke<R, Args...>::invoke(c, m, args);
          });
      })
    );
}

template <typename R, typename ... Args>
static
void
register_method(boost::program_options::options_description& options,
                std::string name,
                R (Client::*m)() const,
                std::vector<std::function<void (Client&)>>& actions)
{
  options.add_options()(
    name.c_str(),
    boost::program_options::value<std::vector<std::string>>()
    ->implicit_value(std::vector<std::string>())->zero_tokens()
    ->notifier(
      [m, name, &actions] (std::vector<std::string> args)
      {
        ELLE_TRACE_SCOPE("enqueue %s(%s)", name, args);
        actions.push_back(
          [name, args, m] (Client& c)
          {
            ELLE_TRACE_SCOPE("invoke %s(%s)", name, args);
            Invoke<R, Args...>::invoke(c, m, args);
          });
      })
    );
}

// Non-const method overload.
template <typename R, typename ... Args>
static
void
register_method(boost::program_options::options_description& options,
                std::string name,
                R (Client::*m)(Args ...),
                std::vector<std::function<void (Client&)>>& actions)
{
  register_method(options, name,
                  reinterpret_cast<R (Client::*)(Args ...) const>(m), actions);
}

static
std::unique_ptr<Client>
parse_options(int argc, char** argv,
              std::vector<std::function<void (Client&)>>& actions)
{
  ELLE_TRACE_SCOPE("parse command line");
  using namespace boost::program_options;
  options_description options("Options");
  options.add_options()
    ("help,h", "display the help")
    ("version,v", "display version")
    (
      "meta,m",
      boost::program_options::value<std::string>()->default_value
      ("meta." BOOST_PP_STRINGIZE(INFINIT_VERSION_MINOR) "."
       BOOST_PP_STRINGIZE(INFINIT_VERSION_MAJOR) ".api.production.infinit.io"),
      "meta host"
    )
    ;
  register_method(options, "login", &Client::login, actions);
  register_method(options, "logout", &Client::logout, actions);
  register_method(options, "register", &Client::register_, actions);
  register_method(options, "trophonius", &Client::trophonius, actions);
  variables_map vm;
  try
  {
    store(parse_command_line(argc, argv, options), vm);
    notify(vm);
  }
  catch (invalid_command_line_syntax const& e)
  {
    throw elle::Error(elle::sprintf("command line error: %s", e.what()));
  }
  if (vm.count("help"))
  {
    std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << options;
    std::cout << std::endl;
    std::cout << "Infinit v" << INFINIT_VERSION << std::endl;
    exit(0); // XXX: use Exit exception
  }
  if (vm.count("version"))
  {
    std::cout << INFINIT_VERSION << std::endl;
    exit(0); // XXX: use Exit exception
  }
  return elle::make_unique<Client>(vm["meta"].as<std::string>());
}

int main(int argc, char** argv)
{
  try
  {
    reactor::Scheduler sched;
    reactor::Thread main(
      sched,
      "main",
      [argc, argv]
      {
        std::vector<std::function<void (Client&)>> actions;
        auto meta_client = parse_options(argc, argv, actions);
        for (auto& action: actions)
          action(*meta_client);
      });
    sched.run();
  }
  catch (std::exception const& e)
  {
    elle::fprintf(std::cerr, "%s: fatal error: %s\n", argv[0], e.what());
    return 1;
  }
}
