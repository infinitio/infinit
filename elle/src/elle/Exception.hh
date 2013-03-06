#ifndef ELLE_EXCEPTION_HH
# define ELLE_EXCEPTION_HH

# include <memory>
# include <stdexcept>

# include <elle/attribute.hh>
# include <elle/Backtrace.hh>
# include <elle/types.hh>

#define ELLE_EXCEPTION(Name)                                                  \
  virtual void raise_and_delete() const                                       \
  {                                                                           \
    Name actual(*this);                                                       \
    delete this;                                                              \
    throw actual;                                                             \
  }                                                                           \
                                                                              \
  virtual                                                                     \
  std::unique_ptr< ::elle::Exception>                                         \
  clone() const                                                               \
  {                                                                           \
    return std::unique_ptr< ::elle::Exception>{new Name{*this}};              \
  }                                                                           \

namespace elle
{
  /// Base class for exception, with backtrace.
  class Exception: public std::runtime_error
  {
  /*-------------.
  | Construction |
  `-------------*/
  public:
    Exception(elle::String const& format);
    Exception(elle::Backtrace const& bt, elle::String const& format);
    ELLE_EXCEPTION(Exception);

  private:
    ELLE_ATTRIBUTE_R(Backtrace, backtrace);
    ELLE_ATTRIBUTE_RW(Exception*, inner_exception);
  };

  std::ostream& operator << (std::ostream& s, Exception const& e);
}

#endif
