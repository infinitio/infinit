#include <station/Host.hh>
#include <station/Station.hh>

namespace station
{
  Host::Host(Station& owner,
             papier::Passport const& passport,
             std::unique_ptr<reactor::network::TCPSocket>&& socket):
    _owner(owner),
    _passport(passport),
    _socket(std::move(socket))
  {}

  Host::~Host()
  {
    ELLE_ASSERT_CONTAINS(this->_owner._hosts, this->passport());
    this->_owner._host_remove(*this);
  }

  reactor::network::TCPSocket&
  Host::socket()
  {
    return *this->_socket;
  }

  void
  Host::print(std::ostream& stream) const
  {
    stream << "Host(" << this->passport() << ")";
  }
}
