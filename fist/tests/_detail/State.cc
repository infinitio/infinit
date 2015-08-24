#include "State.hh"

#include <elle/os/path.hh>
#include <elle/system/home_directory.hh>
#include <fist/tests/server.hh>
#include <fist/tests/_detail/Authority.hh>

extern const std::vector<unsigned char> fingerprint;

namespace tests
{
  State::State(Server& server,
               elle::UUID device_id,
               boost::filesystem::path const& home)
    : _download_dir()
    , _temporary_dir()
    , _state(
      "http", "127.0.0.1", server.port(),
      fingerprint,
      device_id,
      this->_download_dir.path().string(),
      !home.empty() ? home.string() : this->_temporary_dir.path().string(),
      authority
      )
    , _trophonius_fingerprint(fingerprint)
  {
    this->_state.s3_hostname(aws::URL{"http://",
                             elle::sprintf("localhost:%s", server.port()),
                             "/s3"});
  }
}

const std::vector<unsigned char> fingerprint =
{
  0x66, 0x84, 0x68, 0xEB, 0xBE, 0x83, 0xA0, 0x5C, 0x6A, 0x32,
  0xAD, 0xD2, 0x58, 0x62, 0x01, 0x31, 0x79, 0x96, 0x78, 0xB8
};
