#include <cassert>
#include <sstream>

#include <elle/Buffer.hh>

#include <protocol/Packet.hh>

ELLE_LOG_COMPONENT("infinit.protocol.Packet")

namespace infinit
{
  namespace protocol
  {
    class Packet::StreamBuffer: public elle::StreamBuffer, public boost::noncopyable
    {
    public:
      enum class Mode
      {
        fresh,
        eof,
        write,
      };

      StreamBuffer(Packet& p)
        : _mode(Mode::fresh)
        , _packet(p)
        , _size(0)
        , _data(0)
      {}

      StreamBuffer(Packet& p, StreamBuffer&& source)
        : _mode(source._mode)
        , _packet(p)
        , _size(source._size)
        , _data(0)
      {
        // FIXME: we might be able to dodge all this by making
        // elle::StreamBuffer movable and copying the {g,p}ptr() at
        // construction time.
        switch (source._mode)
          {
            case Mode::fresh:
              _data = source._data;
              break;
            case Mode::write:
              // FIXME
              std::abort();
              break;
            case Mode::eof:
              _data = source.gptr();
              _mode = Mode::fresh;
              break;
            default:
              // FIXME: unreachable
              std::abort();
          }
      }

      virtual
      elle::WeakBuffer
      write_buffer()
      {
        if (_mode == Mode::fresh)
        {
          assert(!_packet._data);
          _mode = Mode::write;
        }
        assert(_mode == Mode::write);
        // FIXME: Make buffer grow size parametrizable.
        static const elle::Size growth = 1024;
        _size += growth;
        _packet._data = reinterpret_cast<Packet::Byte*>(realloc(_packet._data, _size)); // XXX check realloc !
        return elle::WeakBuffer(_packet._data + _size - growth, growth);
      }

      void flush(unsigned int size)
      {
        _packet._data_size += size;
      }

      virtual
      elle::WeakBuffer
      read_buffer()
      {
        switch (_mode)
        {
          case Mode::eof:
            ELLE_DEBUG("%s: read end of file", *this);
            return elle::WeakBuffer(nullptr, 0);
          case Mode::fresh:
            _mode = Mode::eof;
            if (_data)
            {
              auto size = _packet._data_size - (_data - _packet._data);
              elle::WeakBuffer res(_data, size);
              ELLE_DEBUG("%s: read partial data: %s", *this, res);
              return res;
            }
            else
            {
              assert(_packet._data);
              elle::WeakBuffer res(_packet._data, _packet._data_size);
              ELLE_DEBUG("%s: read whole data: %s", *this, res);
              return res;
            }
          default:
            std::abort();
        }
      }

    private:
      Mode _mode;
      Packet& _packet;
      elle::Size _size;
      Packet::Byte* _data;
    };

    /*-------------.
    | Construction |
    `-------------*/

    Packet::Packet()
      : elle::IOStream(_streambuffer = new StreamBuffer(*this))
      , _data(0)
      , _data_size(0)
    {}

    Packet::Packet(Packet&& source)
      : elle::IOStream(_streambuffer = new StreamBuffer(*this, std::move(*source._streambuffer)))
      , _data(source._data)
      , _data_size(source._data_size)
    {
      source._data = 0;
      source._data_size = 0;
    }

    Packet::~Packet()
    {
      free(_data);
    }

    /*-----------.
    | Properties |
    `-----------*/

    elle::Size
    Packet::size() const
    {
      return this->_data_size;
    }

    /*--------.
    | Details |
    `--------*/

    Packet::Packet(elle::Size data_size)
      : elle::IOStream(_streambuffer = new StreamBuffer(*this))
      , _data(reinterpret_cast<Byte*>(malloc(data_size)))
      , _data_size(data_size)
    {
      if (!_data)
        throw std::bad_alloc();
    }

    /*----------------.
    | Pretty printing |
    `----------------*/

    std::ostream&
    operator << (std::ostream& stream, Packet const& p)
    {
      stream << "packet of size " << p.size();
      return stream;
    }
  }
}
