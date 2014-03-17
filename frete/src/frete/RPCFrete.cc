#include <elle/Version.hh>

#include <protocol/ChanneledStream.hh>
#include <protocol/RPC.hh>

#include <frete/RPCFrete.hh>
#include <frete/Frete.hh>

#include <version.hh>

namespace frete
{
  RPCFrete::RPCFrete(Frete& frete,
                     infinit::protocol::ChanneledStream& channels):
    _rpc(channels),
    _rpc_count("count", this->_rpc),
    _rpc_full_size("full_size", this->_rpc),
    _rpc_file_size("size", this->_rpc),
    _rpc_path("path", this->_rpc),
    _rpc_read("read", this->_rpc),
    _rpc_set_progress("progress", this->_rpc),
    _rpc_version("version", this->_rpc),
    _rpc_key_code("key_code", this->_rpc),
    _rpc_encrypted_read("encrypted_read", this->_rpc),
    _rpc_finish("finish", this->_rpc)
  {
    this->_rpc_count = std::bind(&Frete::count,
                                 &frete);

    this->_rpc_full_size = std::bind(&Frete::full_size,
                                     &frete);

    this->_rpc_file_size = std::bind(&Frete::file_size,
                                     &frete,
                                     std::placeholders::_1);

    this->_rpc_path = std::bind(&Frete::path,
                                &frete,
                                std::placeholders::_1);

    this->_rpc_read = std::bind(&Frete::read,
                                &frete,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

    this->_rpc_set_progress = std::bind(&Frete::set_progress,
                                        &frete,
                                        std::placeholders::_1);

    this->_rpc_version = std::bind(&Frete::version,
                                   &frete);

    this->_rpc_key_code = std::bind(&Frete::key_code,
                                    &frete);

    this->_rpc_encrypted_read = std::bind(&Frete::encrypted_read,
                                          &frete,
                                          std::placeholders::_1,
                                          std::placeholders::_2,
                                          std::placeholders::_3);

    this->_rpc_finish = std::bind(std::bind(&Frete::finish,
                                            &frete));
  }

  RPCFrete::RPCFrete(infinit::protocol::ChanneledStream& channels):
    _rpc(channels),
    _rpc_count("count", this->_rpc),
    _rpc_full_size("full_size", this->_rpc),
    _rpc_file_size("size", this->_rpc),
    _rpc_path("path", this->_rpc),
    _rpc_read("read", this->_rpc),
    _rpc_set_progress("progress", this->_rpc),
    _rpc_version("version", this->_rpc),
    _rpc_key_code("key_code", this->_rpc),
    _rpc_encrypted_read("encrypted_read", this->_rpc),
    _rpc_finish("finish", this->_rpc)
  {
    this->_rpc_version = []
      {
        return elle::Version(INFINIT_VERSION_MAJOR,
                             INFINIT_VERSION_MINOR,
                             INFINIT_VERSION_SUBMINOR);
      };
  }

  void
  RPCFrete::run()
  {
    this->_rpc.run();
  }

  RPCFrete::VersionRPC::ReturnType
  RPCFrete::version()
  {
    try
    {
      return this->_rpc_version();
    }
    catch (infinit::protocol::RPCError&)
    {
      // Before version 0.8.2, the version RPC did not exist. 0.7 is the oldest
      // public version.
      return elle::Version(0, 7, 0);
    }
  }
}
