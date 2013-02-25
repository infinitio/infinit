#include "Buffer.hh"

#include <elle/assert.hh>
#include <elle/Exception.hh>
#include <elle/IOStream.hh>
#include <elle/log.hh>

#include <elle/format/hexadecimal.hh>

#include <elle/io/Dumpable.hh>

#include <iomanip>
#include <iostream>

ELLE_LOG_COMPONENT("elle.Buffer");

#define ELLE_BUFFER_INITIAL_SIZE (sizeof(void*))

namespace elle
{
  void detail::MallocDeleter::operator ()(void* data)
  {
    ::free(data);
  }

//
// ---------- Buffer ----------------------------------------------------------
//

  // Note that an empty buffer has a valid pointer to a memory region with
  // a size of zero.
  Buffer::Buffer()
    : _contents{static_cast<Byte*>(malloc(ELLE_BUFFER_INITIAL_SIZE))}
    , _size{0}
    , _buffer_size{ELLE_BUFFER_INITIAL_SIZE}
  {
    if (this->_contents == nullptr)
      throw std::bad_alloc();
  }

  Buffer::Buffer(size_t size)
    : _contents(nullptr)
    , _size(size)
    , _buffer_size(size)
  {
    if ((this->_contents =
         static_cast<Byte*>(::malloc(this->_buffer_size))) == nullptr)
      throw std::bad_alloc();
  }

  Buffer::Buffer(ContentPair&& pair)
    : _contents(pair.first.release())
    , _size(pair.second)
    , _buffer_size(pair.second)
  {}

  Buffer::Buffer(Byte const* data,
                 size_t size)
    : _contents(nullptr)
    , _size(0)
    , _buffer_size(0)
  {
    if (size == 0)
      {
        this->_buffer_size = ELLE_BUFFER_INITIAL_SIZE;
        if ((this->_contents =
             static_cast<Byte*>(::malloc(_buffer_size))) == nullptr)
          throw std::bad_alloc();
      }
    else
      this->append(data, size);
  }

  Buffer::Buffer(Buffer&& other)
    : _contents(other._contents)
    , _size(other._size)
    , _buffer_size(other._buffer_size)
  {
    other._contents = nullptr;
#ifdef DEBUG
    other._size = 0;
    other._buffer_size = 0;
#endif
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Buffer)
  {
    this->_contents = nullptr;
    this->_size = 0;
    this->_buffer_size = 0;
  }

  Buffer::~Buffer()
  {
    ::free(this->_contents);
  }

  void Buffer::append(void const* data, size_t size)
  {
    ELLE_ASSERT(data != nullptr || size == 0);

    size_t old_size = this->_size;
    this->size(this->_size + size);
    /// XXX some implementations of memmove does not check for memory overlap
    memmove(this->_contents + old_size, data, size);
    // std::uninitialized_copy(
    //   static_cast<Byte const*>(data),
    //   static_cast<Byte const*>(data) + size,
    //   this->_contents + old_size
  }

  void
  Buffer::size(size_t size)
  {
    if (this->_buffer_size < size)
      {
        size_t next_size = Buffer::_next_size(size);
        void* tmp = ::realloc(_contents, next_size);
        if (tmp == nullptr)
          throw std::bad_alloc();
        this->_contents = static_cast<Byte*>(tmp);
        this->_buffer_size = next_size;
      }
    this->_size = size;
  }

  Buffer::ContentPair
  Buffer::release()
  {
    Byte* new_contents = static_cast<Byte*>(::malloc(ELLE_BUFFER_INITIAL_SIZE));
    if (new_contents == nullptr)
        throw std::bad_alloc{};

    ContentPair res{ContentPtr{this->_contents}, this->_size};

    this->_contents = new_contents;
    this->_size = 0;
    this->_buffer_size = ELLE_BUFFER_INITIAL_SIZE;
    return res;
  }

  OutputBufferArchive
  Buffer::writer()
  {
    return OutputBufferArchive(*this);
  }

  InputBufferArchive
  Buffer::reader() const
  {
    return InputBufferArchive(*this);
  }

  void
  Buffer::dump(const Natural32 margin) const
  {
    Natural32         space = 78 - margin - io::Dumpable::Shift.length();
    String            alignment(margin, ' ');
    Natural32         i;
    Natural32         j;

    std::cout << alignment
              << "[Buffer] "
              << "address(" << static_cast<Void*>(this->_contents) << ") "
              << "size(" << std::dec << this->_size << ") "
              << "capacity(" << std::dec << this->_buffer_size << ")"
              << std::endl;

    // since a byte is represented by two characters.
    space = space / 2;

    // set the fill to '0'.
    std::cout.fill('0');

    // display the region.
    for (i = 0; i < (this->_size / space); i++)
      {
        std::cout << alignment << io::Dumpable::Shift;

        for (j = 0; j < space; j++)
          std::cout << std::nouppercase
                    << std::hex
                    << std::setw(2)
                    << (int)this->_contents[i * space + j];

        std::cout << std::endl;
      }

    if ((this->_size % space) != 0)
      {
        std::cout << alignment << io::Dumpable::Shift;

        for (size_t j = 0; j < (this->_size % space); j++)
          std::cout << std::nouppercase
                    << std::hex
                    << std::setw(2)
                    << (int)this->_contents[i * space + j];

        std::cout << std::endl;
      }
  }

  size_t
  Buffer::_next_size(size_t size)
  {
    if (size < 32)
      return 32;
    if (size < 4096)
      return size *= 2;
    return (size + size  / 2);
  }

  bool
  Buffer::operator <(Buffer const& other) const
  {
    if (this->_size != other._size)
      return this->_size < other._size;
    return ::memcmp(this->_contents, other._contents, this->_size) < 0;
  }

  bool
  Buffer::operator <=(Buffer const& other) const
  {
    if (this->_size != other._size)
      return this->_size < other._size;
    return ::memcmp(this->_contents, other._contents, this->_size) <= 0;
  }

  bool
  Buffer::operator ==(Buffer const& other) const
  {
    if (this->_size != other._size)
      return false;
    return ::memcmp(this->_contents, other._contents, this->_size) == 0;
  }

  bool
  Buffer::operator <(WeakBuffer const& other) const
  {
    if (this->_size != other.size())
      return this->_size < other.size();
    return ::memcmp(this->_contents, other.contents(), this->_size) < 0;
  }

  bool
  Buffer::operator <=(WeakBuffer const& other) const
  {
    if (this->_size != other.size())
      return this->_size < other.size();
    return ::memcmp(this->_contents, other.contents(), this->_size) <= 0;
  }

  bool
  Buffer::operator ==(WeakBuffer const& other) const
  {
    if (this->_size != other.size())
      return false;
    return ::memcmp(this->_contents, other.contents(), this->_size) == 0;
  }

  std::ostream&
  operator <<(std::ostream& stream,
              Buffer const& buffer)
  {
    static Natural32 const length = 40;
    String hexadecimal = format::hexadecimal::encode(buffer);

    // Display the string, depending on its length.
    if (hexadecimal.length() == 0)
      {
        stream << "empty";
      }
    else if (hexadecimal.length() < length)
      {
        // If the string is short enough, display it in its entirety.
        stream << hexadecimal;
      }
    else
      {
        // Otherwise chop it and display the begining and the end only.
        stream << hexadecimal.substr(0, length / 2)
               << "..." << std::dec << buffer.size() << " bytes" << "..."
               << hexadecimal.substr(hexadecimal.length() - (length / 2));
      }

    return (stream);
  }

//
// ---------- WeakBuffer ------------------------------------------------------
//

  InputBufferArchive
  WeakBuffer::reader() const
  {
    return InputBufferArchive(*this);
  }

  void
  WeakBuffer::dump(const Natural32 margin) const
  {
    Natural32         space = 78 - margin - io::Dumpable::Shift.length();
    String            alignment(margin, ' ');
    Natural32         i;
    Natural32         j;

    std::cout << alignment
              << "[Buffer] "
              << "address(" << static_cast<Void*>(this->_contents) << ") "
              << "size(" << std::dec << this->_size << ")"
              << std::endl;

    // since a byte is represented by two characters.
    space = space / 2;

    // set the fill to '0'.
    std::cout.fill('0');

    // display the region.
    for (i = 0; i < (this->_size / space); i++)
      {
        std::cout << alignment << io::Dumpable::Shift;

        for (j = 0; j < space; j++)
          std::cout << std::nouppercase
                    << std::hex
                    << std::setw(2)
                    << (int)this->_contents[i * space + j];

        std::cout << std::endl;
      }

    if ((this->_size % space) != 0)
      {
        std::cout << alignment << io::Dumpable::Shift;

        for (size_t j = 0; j < (this->_size % space); j++)
          std::cout << std::nouppercase
                    << std::hex
                    << std::setw(2)
                    << (int)this->_contents[i * space + j];

        std::cout << std::endl;
      }
  }

  bool
  WeakBuffer::operator <(WeakBuffer const& other) const
  {
    if (this->_size != other._size)
      return this->_size < other._size;
    return ::memcmp(this->_contents, other._contents, this->_size) < 0;
  }

  bool
  WeakBuffer::operator <=(WeakBuffer const& other) const
  {
    if (this->_size != other._size)
      return this->_size < other._size;
    return ::memcmp(this->_contents, other._contents, this->_size) <= 0;
  }

  bool
  WeakBuffer::operator ==(WeakBuffer const& other) const
  {
    if (this->_size != other._size)
      return false;
    return ::memcmp(this->_contents, other._contents, this->_size) == 0;
  }

  bool
  WeakBuffer::operator <(Buffer const& other) const
  {
    if (this->_size != other.size())
      return this->_size < other.size();
    return ::memcmp(this->_contents, other.contents(), this->_size) < 0;
  }

  bool
  WeakBuffer::operator <=(Buffer const& other) const
  {
    if (this->_size != other.size())
      return this->_size < other.size();
    return ::memcmp(this->_contents, other.contents(), this->_size) <= 0;
  }

  bool
  WeakBuffer::operator ==(Buffer const& other) const
  {
    if (this->_size != other.size())
      return false;
    return ::memcmp(this->_contents, other.contents(), this->_size) == 0;
  }

  /*----------.
  | Operators |
  `----------*/

  std::ostream&
  operator <<(std::ostream& stream,
              WeakBuffer const& buffer)
  {
    static Natural32 const length = 40;
    String hexadecimal = format::hexadecimal::encode(buffer);

    // Display the string, depending on its length.
    if (hexadecimal.length() == 0)
      {
        stream << "empty";
      }
    else if (hexadecimal.length() < length)
      {
        // If the string is short enough, display it in its entirety.
        stream << hexadecimal;
      }
    else
      {
        // Otherwise chop it and display the begining and the end only.
        stream << hexadecimal.substr(0, length / 2)
               << "..." << std::dec << buffer.size() << " bytes" << "..."
               << hexadecimal.substr(hexadecimal.length() - (length / 2));
      }

    return (stream);
  }

  ///////////////////////////////////////////////////////////////////////////

  template<typename BufferType>
  class InputStreamBuffer:
    public StreamBuffer
  {
  private:
    BufferType const& _buffer;
    size_t            _index;

  public:
    explicit
    InputStreamBuffer(BufferType const& buffer):
      _buffer(buffer),
      _index(0)
    {
      ELLE_DEBUG("create an InputStreamBuffer on a buffer of size %s", buffer.size());
    }

    WeakBuffer
    write_buffer()
    {
      throw Exception("the buffer is in input mode");
    }

    WeakBuffer
    read_buffer()
    {
      ELLE_DEBUG(
        "read from an InputStreamBuffer at index %s (bytes left = %s)",
        _index, _buffer.size() - _index
      );
      assert(_index <= _buffer.size());
      return WeakBuffer(
        (char*)_buffer.contents() + _index, _buffer.size() - _index
      );
    }

    void flush(Size size)
    {
      ELLE_DEBUG("flush %s bytes from index %s of an InputStreamBuffer", size, _index);
      _index += size;
      assert(_index <= _buffer.size());
    }

  };

  ///////////////////////////////////////////////////////////////////////////

  InputBufferArchive::InputBufferArchive(WeakBuffer const& buffer):
    serialize::InputBinaryArchive(
      *(_istream = new IOStream(new InputStreamBuffer<WeakBuffer>(buffer)))
    )
  {}

  InputBufferArchive::InputBufferArchive(Buffer const& buffer):
    serialize::InputBinaryArchive(
      *(_istream = new IOStream(new InputStreamBuffer<Buffer>(buffer)))
    )
  {}

  InputBufferArchive::InputBufferArchive(InputBufferArchive&& other):
    serialize::InputBinaryArchive(*_istream),
    _istream(other._istream)
  {
    other._istream = nullptr;
  }

  InputBufferArchive::~InputBufferArchive()
  {
    delete _istream;
    _istream = nullptr;
  }

  ///////////////////////////////////////////////////////////////////////////

  class OutputStreamBuffer:
    public StreamBuffer
  {
  private:
    size_t  _old_size;
    Buffer& _buffer;

  public:
    OutputStreamBuffer(Buffer& buffer):
      _old_size(buffer.size()),
      _buffer(buffer)
    {}

    WeakBuffer
    write_buffer()
    {
      _buffer.size(_old_size + 512);
      ELLE_DEBUG("Grow stream buffer from %s to %s bytes", _old_size, _buffer.size());
      return WeakBuffer(
          (char*)_buffer.mutable_contents() + _old_size,
          512
      );
    }

    void
    flush(Size size)
    {
      ELLE_DEBUG("Flush buffer stream size: %s", size);
      _old_size += size;
      _buffer.size(_old_size);
    }

    WeakBuffer
    read_buffer()
    {
      throw Exception("the buffer is in output mode");
    }
  };

  ///////////////////////////////////////////////////////////////////////////

  OutputBufferArchive::OutputBufferArchive(Buffer& buffer):
    serialize::OutputBinaryArchive(
      *(_ostream = new IOStream(new OutputStreamBuffer(buffer)))
    )
  {
    ELLE_DEBUG("create OutputBufferArchive %s stream %s", this, _ostream);
  }

  OutputBufferArchive::OutputBufferArchive(OutputBufferArchive&& other):
    serialize::OutputBinaryArchive(*_ostream),
    _ostream(other._ostream)
  {
    ELLE_DEBUG("move OutputBufferArchive %s stream %s", this, _ostream);
    other._ostream = nullptr;
  }

  OutputBufferArchive::~OutputBufferArchive()
  {
    ELLE_DEBUG("Delete OutputBufferArchive %s stream %s", this, _ostream);
    delete _ostream;
    _ostream = nullptr;
  }

}
