#ifndef  ELLE_PRINT_HXX
# define ELLE_PRINT_HXX

# include <elle/Printable.hh>

# include <iostream>
# include <sstream>
# include <typeinfo>

namespace elle
{

  namespace detail
  {

    template <typename K>
    struct Clean
    {
      typedef typename std::remove_cv<
        typename std::remove_reference<K>::type
      >::type type;
    };

    template <typename _OStream, typename _T>
    struct IsPrintable
    {
    private:
      typedef typename Clean<_OStream>::type  OStream;
      typedef typename Clean<_T>::type        T;
      typedef char No;
      typedef struct { No _[2];} Yes;

      static
      No
      f(...);

      template <size_t>
      struct Helper
      {};

      template <typename U>
      static
      Yes
      f(U*,
        Helper<sizeof(*((OStream*) 0) << *((U*)0))>* sfinae = 0);
    public:
      static bool const value = (sizeof f((T*)0) == sizeof(Yes));
    };

    struct PrintFlags
    {
    public:
      std::string endl;
      std::string sep;

    public:
      PrintFlags():
        endl("\n"),
        sep(" ")
      {}
    };

# define _ELLE_PRINT_HAS_OVERRIDES(__type) (                                  \
      std::is_same<                                                           \
          elle::iomanip::Separator                                            \
        , typename Clean<T>::type                                             \
      >::value                                                                \
  ||  std::is_same<                                                           \
          elle::iomanip::EndOfLine                                            \
        , typename Clean<T>::type                                             \
      >::value                                                                \
  ||  std::is_base_of<                                                        \
          elle::Printable                                                     \
        , typename Clean<T>::type                                             \
      >::value                                                                \
)                                                                             \
/**/

    // generic value fprint
    template <typename T>
    typename std::enable_if<
        IsPrintable<std::ostream, T>::value && !_ELLE_PRINT_HAS_OVERRIDES(T)
      , bool
    >::type
    fprint_value(std::ostream&                      out,
                 PrintFlags&                        flags,
                 bool                               is_first,
                 T&&                                value)
    {
      if (!is_first)
          out << flags.sep;
      out << value;
      return false;
    }

    template<typename T>
    typename std::enable_if<
        !IsPrintable<std::ostream, T>::value && !_ELLE_PRINT_HAS_OVERRIDES(T)
      , bool
    >::type
    fprint_value(std::ostream&                      out,
                 PrintFlags&                        flags,
                 bool                               is_first,
                 T&&                                value)
    {
      if (!is_first)
          out << flags.sep;
      out << '<' << typeid(T).name() << "instance at "
          << std::hex << static_cast<void const*>(&value) << '>';
      return false;
    }

    // Specialization to treat printable classes
    bool
    fprint_value(std::ostream&                      out,
                 PrintFlags&                        flags,
                 bool                               is_first,
                 elle::Printable const&             value);

    // specialization to treat separator classes
    bool
    fprint_value(std::ostream&                      out,
                 PrintFlags&                        flags,
                 bool                               is_first,
                 elle::iomanip::Separator const&    value);

    // specialization to treat end of line classes
    bool
    fprint_value(std::ostream&                      out,
                 PrintFlags&                        flags,
                 bool                               is_first,
                 elle::iomanip::EndOfLine const&    value);

    // fprint recursion ends here
    void
    fprint(std::ostream&                            out,
           PrintFlags&                              flags,
           bool                                     is_first);

    // Generic fprint
    template <typename T, typename... U>
    void
    fprint(std::ostream&                            out,
           PrintFlags&                              flags,
           bool                                     is_first,
           T&&                                      value,
           U&&...                                   values)
    {
      is_first = fprint_value(out, flags, is_first, std::forward<T>(value));
      fprint(out, flags, is_first, std::forward<U>(values)...);
    }

  } // !elle::detail

  template <typename... T>
  void
  print(T&&...         values)
  {
    return fprint(std::cout, std::forward<T>(values)...);
  }

  template <typename... T>
  void
  fprint(std::ostream&      out,
         T&&...             values)
  {
    out << std::dec;
    elle::detail::PrintFlags flags;
    return ::elle::detail::fprint(out, flags, true, std::forward<T>(values)...);
  }

  template <typename... T>
  std::string
  sprint(T&&...     values)
  {
    std::ostringstream ss;
    static iomanip::EndOfLine const nonewline('\0');
    fprint(ss, nonewline, std::forward<T>(values)...);
    return ss.str();
  }

}

#endif
