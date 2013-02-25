#ifndef INFINIT_REACTOR_EXCEPTION_HH
# define INFINIT_REACTOR_EXCEPTION_HH

# include <stdexcept>

# include <elle/Backtrace.hh>

# include <reactor/fwd.hh>

#define INFINIT_REACTOR_EXCEPTION(Name)         \
  virtual void raise_and_delete() const         \
  {                                             \
    Name actual(*this);                         \
    delete this;                                \
    throw actual;                               \
  }

namespace reactor
{
  class Exception: public std::runtime_error
  {
  public:
    Exception(const std::string& message);
    Exception(const std::string& message,
              elle::Backtrace const& bt);
    virtual ~Exception() throw ();
    INFINIT_REACTOR_EXCEPTION(Exception);
    elle::Backtrace const& backtrace() const;
    Exception const* inner_exception() const;
    void inner_exception(Exception* e);
    void raise();
  private:
    elle::Backtrace _backtrace;
    Exception* _inner;
  };

  std::ostream& operator << (std::ostream& s, Exception const& e);

  class Terminate: public Exception
  {
  public:
    typedef Exception Super;
    Terminate();
    INFINIT_REACTOR_EXCEPTION(Terminate);
  };
}

#endif
