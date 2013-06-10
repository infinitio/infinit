#include <reactor/scheduler.hh>
#include <reactor/network/udt-socket.hh>
#include <reactor/network/udp-socket.hh>
#include <reactor/network/udt-rdv-server.hh>
#include <reactor/network/resolve.hh>
#include <reactor/network/nat.hh>
#include <reactor/network/buffer.hh>

#include <elle/log.hh>

#include <common/common.hh>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

ELLE_LOG_COMPONENT("test.pass");

void
Main()
{
  auto& sched = *reactor::Scheduler::scheduler();
  reactor::nat::NAT nat(sched);
  boost::asio::ip::udp::endpoint pub;
  std::unique_ptr<reactor::network::UDPSocket> socket;

  auto lhost = reactor::network::resolve_udp(sched, common::longinus::host(),
                                             std::to_string(3478));
  auto breach = nat.map(lhost);

  using reactor::nat::Breach;

  if (breach.nat_behavior() == Breach::NatBehavior::EndpointIndependentMapping ||
      breach.nat_behavior() == Breach::NatBehavior::DirectMapping)
  {
    ELLE_TRACE("breach done: %s", breach.mapped_endpoint());
    socket = std::move(breach.take_handle());
    pub = breach.mapped_endpoint();
  }
  else
    throw elle::Exception{"invalid mapping behavior"};

  elle::print(pub);
  reactor::network::UDTRendezVousServer serv(sched, std::move(socket));
  std::string addr;
  std::getline(std::cin, addr);
  std::string host, port;
  {
    std::vector<std::string> v;
    boost::split(v, addr, boost::is_any_of(":"));
    host = v[0];
    port = v[1];
  }
  std::cout << host << ":" << port << std::endl;
  serv.accept(host, std::stoi(port));
  std::unique_ptr<reactor::network::UDTSocket> client{serv.accept()};
  {
    std::string msg("coucou\n");
    client->write(reactor::network::Buffer{msg});
    msg.resize(64);
    size_t b = client->read_some(reactor::network::Buffer{msg});
    msg.resize(b);
    if (msg == "coucou\n")
      std::cout << "Yeah !!\n";
    else
      std::cout << "Oh Nose!!\n";
  }
}

int
main()
{
  reactor::Scheduler sched;

  reactor::Thread run(sched, "main", Main);
  sched.run();
  return 0;
}
