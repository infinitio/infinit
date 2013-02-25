#include <ostream>

#include "Array.hh"
#include "Null.hh"

namespace elle
{
  namespace format
  {
    namespace json
    {

      Array::Array():
        _value{}
      {}

      Array::Array(std::vector<Object*>&& value):
        _value{value}
      {}

      Array::~Array()
      {
        for (auto it = _value.begin(), end = _value.end(); it != end; ++it)
          delete (*it);
        _value.clear();
      }

      void
      Array::push_back(std::unique_ptr<Object>&& value)
      {
        assert(value.get() != nullptr);
        _value.push_back(value.get());
        value.release();
      }

      void
      Array::repr(std::ostream& out) const
      {
        out << '[';
        bool first{true};
        for (Object const* element : _value)
          {
            if (first)
              first = false;
            else
              out << ',';
            element->repr(out);
          }
        out << ']';
      }

      std::unique_ptr<Object>
      Array::clone() const
      {
        auto res = std::unique_ptr<Array>(new Array);
        for (auto it = _value.begin(), end = _value.end(); it != end; ++it)
          {
            res->_value.push_back((*it)->clone().release());
          }
        return std::unique_ptr<Object>(res.release());
      }

      bool
      Array::operator ==(Object const& other) const
      {
        return other == *this;
      }

      bool
      Array::operator ==(Array const& other) const
      {
        if (this->size() != other.size())
          return false;
        for (size_t i = 0; i < this->size(); ++i)
          if (*_value[i] != *other._value[i])
            return false;
        return true;
      }

    }
  }
}
