#include <boost/foreach.hpp>

#include <elle/log.hh>

#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

#include <cryptography/random.hh>

#include <protocol/Channel.hh>
#include <protocol/ChanneledStream.hh>

ELLE_LOG_COMPONENT("infinit.protocol.Channel");

namespace infinit
{
  namespace protocol
  {
    /*-------------.
    | Construction |
    `-------------*/

    bool
    ChanneledStream::_handshake(Stream& backend)
    {
      while (true)
      {
        ELLE_TRACE_SCOPE("%s: handshake to determine master", *this);
        char mine = infinit::cryptography::random::generate<char>();
        char his;
        {
          Packet p;
          p << mine;
          backend.write(p);
          ELLE_DEBUG("%s: my roll: %d", *this, (int)mine);
        }
        {
          Packet p(backend.read());
          p >> his;
          ELLE_DEBUG("%s: his roll: %d", *this, (int)his);
        }
        if (mine != his)
        {
          bool master = mine > his;
          ELLE_TRACE("%s: %s", *this, master ? "master" : "slave");
          return master;
        }
        else
          ELLE_DEBUG("rolls are equal, restart handshake");
      }
    }

    ChanneledStream::ChanneledStream(reactor::Scheduler& scheduler,
                                     Stream& backend)
      : Super(scheduler)
      , _master(this->_handshake(backend))
      , _id_current(0)
      , _reading(false)
      , _backend(backend)
      , _channels()
      , _channels_new()
      , _channel_available()
      , _default(*this)
    {}

    /*----.
    | IDs |
    `----*/

    int
    ChanneledStream::_id_generate()
    {
      int res = _id_current;
      if (_master)
        {
          ++_id_current;
          if (_id_current < 0)
            _id_current = 1;
        }
      else
        {
          --_id_current;
          if (_id_current > 0)
            _id_current = -1;
        }
      return res;
    }

    /*----------.
    | Receiving |
    `----------*/

    Packet
    ChanneledStream::read()
    {
      return _default.read();
    }

    Packet
    ChanneledStream::_read(Channel* channel)
    {
      ELLE_TRACE_SCOPE("%s: read packet on channel %s", *this, channel->_id);
      int requested_channel = channel->_id;
      reactor::Thread* current = scheduler().current();
      while (true)
        {
          if (!channel->_packets.empty())
            {
              // FIXME: use helper to pop
              Packet packet(std::move(channel->_packets.front()));
              channel->_packets.pop_front();
              ELLE_TRACE("%s: %s available.", *this, packet);
              return packet;
            }
          ELLE_DEBUG("%s: no packet available.", *this);
          if (!_reading)
            _read(false, requested_channel);
          else
            ELLE_DEBUG("%s: reader already present, waiting.", *this)
              current->wait(channel->_available);
        }
    }

    void
    ChanneledStream::_read(bool new_channel, int requested_channel)
    {
      ELLE_TRACE_SCOPE("%s: reading packets.", *this);
      try
      {
        while (true)
        {
          _reading = true;
          Packet p(_backend.read());
          if (p.size() < 4)
            throw elle::Exception("packet is too small for channel id");
          int channel_id = _uint32_get(p);
          // FIXME: The size of the packet isn't
          // adjusted. This is cosmetic though.
          auto it = _channels.find(channel_id);
          if (it != _channels.end())
          {
            ELLE_DEBUG("%s: received %s on existing %s.",
                       *this, p, *it->second);
            it->second->_packets.push_back(std::move(p));
            if (channel_id == requested_channel)
              break;
            else
              it->second->_available.signal_one();
          }
          else
          {
            Channel res(*this, channel_id);
            ELLE_DEBUG("%s: received %s on brand new %s.", *this, p, res);
            res._packets.push_back(std::move(p));
            _channels_new.push_back(std::move(res));
            if (new_channel)
              break;
            else
              _channel_available.signal_one();
          }
        }
        // Wake another thread so it can read future packets.
        _reading = false;
        for (auto channel: _channels)
          if (channel.second->_available.signal_one())
            return;
        _channel_available.signal_one();
      }
      catch (...)
      {
        // Wake another thread so it fails too.
        ELLE_DEBUG_SCOPE("%s: read failed, wake next thread.", *this);
        _reading = false;
        bool woken = false;
        for (auto channel: _channels)
          if (channel.second->_available.signal_one())
          {
            woken = true;
            break;
          }
        if (!woken)
          _channel_available.signal_one();
        throw;
      }
    }

    Channel
    ChanneledStream::accept()
    {
      ELLE_TRACE_SCOPE("%s: wait for incoming channel", *this);
      while (_channels_new.empty())
        {
          ELLE_DEBUG("%s: no channel available, waiting", *this);
          if (!_reading)
            _read(true, 0);
          else
            {
              ELLE_DEBUG("%s: reader already present, waiting.", *this);
              reactor::Thread* current = scheduler().current();
              current->wait(this->_channel_available);
            }
        }
      assert(!_channels_new.empty());
      // FIXME: use helper to pop
      Channel res = std::move(_channels_new.front());
      _channels_new.pop_front();
      ELLE_TRACE("%s: got %s", *this, res);
      return res;
    }

    /*--------.
    | Sending |
    `--------*/

    void
    ChanneledStream::_write(Packet& packet)
    {
      _default.write(packet);
    }

    void
    ChanneledStream::_write(Packet& packet, int id)
    {
      ELLE_TRACE_SCOPE("%s: send %s on channel %s", *this, packet, id);

      Packet backend_packet;
      _uint32_put(backend_packet, id);
      backend_packet.write(packet._data, packet.size());
      _backend.write(backend_packet);
    }

    /*----------.
    | Printable |
    `----------*/

    void
    ChanneledStream::print(std::ostream& stream) const
    {
      stream << "ChanneledStream " << this;
    }
  }
}
