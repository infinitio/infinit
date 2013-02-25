#include <elle/network/Interface.hh>
#include <elle/utility/Suffixes.hh>

#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>

#ifdef AF_LINK // BSD systems
#   include <net/if_dl.h>
#endif
#ifdef AF_PACKET // linux
#   include <netpacket/packet.h>
#endif

#include <stdio.h>


#ifdef AF_PACKET
# define hw_addr_family AF_PACKET
# define hw_addr_type sockaddr_ll
# define hw_addr_len(addr) addr->sll_halen
# define hw_addr_data(addr) addr->sll_addr
#else
# define hw_addr_family AF_LINK
# define hw_addr_type sockaddr_dl
# define hw_addr_len(addr) (addr->sdl_alen)
# define hw_addr_data(addr) (addr->sdl_data + addr->sdl_nlen)
#endif

namespace elle
{
  namespace network
  {

    namespace {
      bool
      check_ipv4_autoip(struct sockaddr *sock)
      {
        using namespace elle::suffix;
        // 169.254.0.0
        static const auto autoip_addr    = 10101001111111100000000000000000_bits;
        // 255.255.0.0
        static const auto autoip_netmask = 11111111111111110000000000000000_bits;

        // This check only make sense in ipv4.
        if (sock->sa_family != AF_INET)
          return false;

        struct sockaddr_in *sin = (struct sockaddr_in *)sock;
        auto addr_bits = std::bitset<32>(ntohl(sin->sin_addr.s_addr));

        return autoip_addr == (autoip_netmask & addr_bits);
      }
    }
    std::map<std::string, Interface>
    Interface::get_map(Interface::Filter filter)
    {
      std::map<std::string, Interface> map;

      ifaddrs * ifap = 0;
      if (getifaddrs(&ifap) != 0)
        return map;

      for (ifaddrs* iter = ifap; iter != NULL; iter = iter->ifa_next)
      {
        // Apply filters
        if (filter & Interface::Filter::no_loopback &&
            bool(iter->ifa_flags & IFF_LOOPBACK))
          continue;

        if (filter & Interface::Filter::only_up &&
            !(iter->ifa_flags & IFF_UP))
          continue;

        if ((filter & Interface::Filter::no_autoip) &&
            check_ipv4_autoip(iter->ifa_addr))
          continue;

        std::ostringstream oss;
        uint8_t* tab;
        sockaddr_in* inet_addr;
        hw_addr_type* hw_addr;
        switch (iter->ifa_addr->sa_family)
        {
        case AF_INET:
          inet_addr = reinterpret_cast<sockaddr_in*>(iter->ifa_addr);
          tab = reinterpret_cast<uint8_t*>(&inet_addr->sin_addr.s_addr);
          for (size_t i = 0; i < sizeof(inet_addr->sin_addr.s_addr); ++i)
          {
            if (i != 0)
              oss << '.';
            oss << static_cast<unsigned int>(tab[i]);
          }
          map[iter->ifa_name].ipv4_address = oss.str();
          break;
        case hw_addr_family:
          hw_addr = reinterpret_cast<hw_addr_type*>(iter->ifa_addr);
          for (size_t i = 0; i < hw_addr_len(hw_addr); ++i)
          {
            if (i != 0)
              oss << ':';
            oss << std::setfill('0') << std::setw(2) << std::hex
                << static_cast<unsigned int>((uint8_t)hw_addr_data(hw_addr)[i]);
          }
          map[iter->ifa_name].mac_address = oss.str();
          break;
        }
      }
      freeifaddrs(ifap);
      return map;
    }

  }
}

