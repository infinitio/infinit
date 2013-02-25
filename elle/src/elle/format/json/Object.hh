#ifndef ELLE_FORMAT_JSON_OBJECT_HH
# define ELLE_FORMAT_JSON_OBJECT_HH

# include <cstdint>
# include <iosfwd>
# include <memory>
# include <string>

# include "fwd.hh"

namespace elle
{
  namespace format
  {
    namespace json
    {

      /// Root object for all json types
      class Object
      {
      public:
        virtual
        ~Object()
        {}

      public:
        /// Get the value of an object value, when the object value
        /// can be casted without precision loss into the type T.
        /// For example, float to integer conversion WON'T work.
        ///
        /// @throws std::bad_cast.
        /// @see Object::TryLoad() for an exception safe version.
        template <typename T>
        void
        load(T& out) const;

        /// Same as Load(), but instead of throwing an exception,
        /// it returns false when conversion cannot be done.
        template <typename T>
        bool
        try_load(T& out) const;

        /// Convert an object to the type T and return by value.
        /// @throws std::bad_cast
        template <typename T>
        T
        as() const;

        /// Cast to a specific json type.
        /// @throws std::bad_cast
        Array&
        as_array();
        Bool&
        as_bool();
        Dictionary&
        as_dictionary();
        Float&
        as_float();
        Integer&
        as_integer();
        Null&
        as_null();
        String&
        as_string();
        Array const&
        as_array() const;
        Bool const&
        as_bool() const;
        Dictionary const&
        as_dictionary() const;
        Float const&
        as_float() const;
        Integer const&
        as_integer() const;
        Null const&
        as_null() const;
        String const&
        as_string() const;


        /// Cloning a JSON Object
        virtual
        std::unique_ptr<Object>
        clone() const = 0;

        /// Returns the JSON representation of this object
        std::string
        repr() const;

        /// Write the JSON representation into a stream.
        virtual
        void
        repr(std::ostream& out) const = 0;


      public:
        /// operator ==
        virtual
        bool
        operator ==(Object const& other) const = 0;
        virtual
        bool
        operator ==(Array const&) const;
        virtual
        bool
        operator ==(Bool const&) const;
        virtual
        bool
        operator ==(Dictionary const&) const;
        virtual
        bool
        operator ==(Float const&) const;
        virtual
        bool
        operator ==(Integer const&) const;
        virtual
        bool
        operator ==(Null const&) const;
        virtual
        bool
        operator ==(String const&) const;
        template <typename T>
        typename std::enable_if<!std::is_base_of<T, Object>::value, bool>::type
        operator ==(T const& other) const;

        /// operator !=
        inline
        bool
        operator !=(Object const& other) const;
        inline
        bool
        operator !=(Array const& other) const;
        inline
        bool
        operator !=(Bool const& other) const;
        inline
        bool
        operator !=(Dictionary const& other) const;
        inline
        bool
        operator !=(Float const& other) const;
        inline
        bool
        operator !=(Integer const& other) const;
        inline
        bool
        operator !=(Null const& other) const;
        inline
        bool
        operator !=(String const& other) const;
        template <typename T>
        inline
        bool
        operator !=(T const& other) const;

        /// Usable as a constant dictionary
        virtual Object const& operator [](std::string const& key) const;

        /// Usable as a constant array
        virtual Object const& operator [](size_t index) const;

      protected:
        // Array and Dictionary class may save children objects.
        friend class Dictionary;
        friend class Array;
      };

    }
  }
}

# include "Object.hxx"

#endif /* ! OBJECT_HH */
