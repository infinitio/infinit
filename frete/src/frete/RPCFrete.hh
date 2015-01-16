#ifndef FRETE_RPCFRETE_HH
# define FRETE_RPCFRETE_HH

# include <functional>

# include <elle/Version.hh>
# include <elle/attribute.hh>
# include <elle/serialize/PairSerializer.hxx>
# include <elle/serialize/VectorSerializer.hxx>

# include <protocol/fwd.hh>

# include <frete/Frete.hh>

namespace frete
{
// Prepend the name of the attribute with rpc_ to create the name of the rpc.
#define RPC_NAME(_name_)                                                \
  BOOST_PP_CAT(rpc_, _name_)

// Create a getter that use the rpc.
// RPC_WRAPPER(Foo, foo) will results to:
// An attribute: Foo _rpc_foo;
// A method foo(...) { return _rpc_foo(_ARGS_); }
#define RPC_WRAPPER(_rpc_type_, _name_)                                 \
  ELLE_ATTRIBUTE_X(_rpc_type_, RPC_NAME(_name_));                       \
  public:                                                               \
  template <typename... Args>                                           \
  _rpc_type_::ReturnType                                                \
  _name_(Args&&... args)                                                \
  {                                                                     \
    return this->RPC_NAME(_name_)()(std::forward<Args&&>(args)...);      \
  }

  class RPCFrete
  {
  private:
  /*------.
  | Types |
  `------*/
    typedef infinit::protocol::RPC<
      elle::serialize::InputBinaryArchive,
      elle::serialize::OutputBinaryArchive> RPC;
    typedef RPC::RemoteProcedure<Frete::FileCount> CountRPC;
    typedef RPC::RemoteProcedure<Frete::FileSize> FullSizeRPC;
    typedef RPC::RemoteProcedure<Frete::FileSize, Frete::FileID> FileSizeRPC;
    typedef RPC::RemoteProcedure<std::vector<std::pair<std::string, Frete::FileSize>>> FilesInfoRPC;
    typedef RPC::RemoteProcedure<std::string, Frete::FileID> FilePathRPC;
    typedef RPC::RemoteProcedure<infinit::cryptography::Code,
                                 Frete::FileID,
                                 Frete::FileOffset,
                                 Frete::FileSize> ReadRPC;
    typedef ReadRPC EncryptedReadRPC;
    typedef RPC::RemoteProcedure<void, Frete::FileSize> SetProgressRPC;
    typedef RPC::RemoteProcedure<elle::Version> VersionRPC;
    typedef RPC::RemoteProcedure<infinit::cryptography::Code> KeyCodeRPC;
    typedef RPC::RemoteProcedure<void> FinishRPC;
    typedef RPC::RemoteProcedure<infinit::cryptography::Code,
                                 Frete::FileID,
                                 Frete::FileOffset,
                                 Frete::FileSize,
                                 Frete::FileSize> EncryptedReadAcknowledgeRPC;
    typedef RPC::RemoteProcedure<Frete::TransferInfo> TransferInfoRPC;
  /*-------------.
  | Construction |
  `-------------*/
  public:
    RPCFrete(Frete& frete,
             infinit::protocol::ChanneledStream& channels);

    RPCFrete(infinit::protocol::ChanneledStream& channels);

    void
    run(infinit::protocol::ExceptionHandler exception_handler = {});
  /*-----------.
  | Attributes |
  `-----------*/
  private:
    ELLE_ATTRIBUTE(RPC, rpc);

  private:
    RPC_WRAPPER(CountRPC, count);
    RPC_WRAPPER(FullSizeRPC, full_size);
    RPC_WRAPPER(FileSizeRPC, file_size);
    RPC_WRAPPER(FilePathRPC, path);
    RPC_WRAPPER(ReadRPC, read);
    RPC_WRAPPER(SetProgressRPC, set_progress);
  private:
    VersionRPC _rpc_version;
  public:
    VersionRPC::ReturnType
    version();
   private:
    RPC_WRAPPER(KeyCodeRPC, key_code);
    RPC_WRAPPER(EncryptedReadRPC, encrypted_read);
    RPC_WRAPPER(FinishRPC, finish);
    RPC_WRAPPER(FilesInfoRPC, files_info);
    RPC_WRAPPER(EncryptedReadAcknowledgeRPC, encrypted_read_acknowledge);
    RPC_WRAPPER(TransferInfoRPC, transfer_info);
  };
}


#endif
