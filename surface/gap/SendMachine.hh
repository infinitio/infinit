#ifndef SURFACE_GAP_SEND_MACHINE_HH
# define SURFACE_GAP_SEND_MACHINE_HH

# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    class SendMachine:
      virtual public TransactionMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef SendMachine Self;
      typedef TransactionMachine Super;

    /*-------------.
    | Construction |
    `-------------*/
    protected:
      SendMachine(Transaction& transaction,
                  uint32_t id,
                  std::vector<std::string> files,
                  std::shared_ptr<Data> data);
    public:
      virtual
      ~SendMachine();

    /*-------------.
    | Plain upload |
    `-------------*/
    public:
      virtual
      float
      progress() const override;
    protected:
      // cleartext upload one file to cloud
      void
      _plain_upload();
      ELLE_ATTRIBUTE(float, plain_progress);
      typedef std::unordered_map<int, float> PlainProgressChunks;
      ELLE_ATTRIBUTE(PlainProgressChunks, plain_progress_chunks);

    /*----.
    | FSM |
    `----*/
    protected:
      virtual
      void
      _create_transaction() = 0;
      reactor::fsm::State& _create_transaction_state;

      // These functions are implemented by subclasses so that the correct
      // metrics are sent.
      virtual
      void
      cancel() = 0;
      virtual
      void
      _fail() = 0;

    /*-----------------.
    | Transaction data |
    `-----------------*/
    public:
      typedef std::vector<std::string> Files;
      ELLE_ATTRIBUTE_R(Files, files);

    public:
      virtual
      bool
      is_sender() const override
      {
        return true;
      }

      /// Return (name, is_an_archive) if we have to put all transfer data
      /// in one file
      std::pair<std::string, bool>
      archive_info();

    protected:
      /* If possible, copy _files to a safer place out of reach of the user,
      * and update _files accordingly.
      * Must be called *before* the first frete creation/snapshoting (where
      * _files is expanded to yield the definitive file list).
      */
      void try_mirroring_files(frete::Frete::FileSize total_size);
      virtual
      void cleanup () override;
    };
  }
}

#endif
