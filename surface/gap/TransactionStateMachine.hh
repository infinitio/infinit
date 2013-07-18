#ifndef SURFACE_GAP_TRANSACTIONSTATEMACHINE_HH
# define SURFACE_GAP_TRANSACTIONSTATEMACHINE_HH

# include "Device.hh"
# include "Self.hh"
# include "Transaction.hh"

# include <elle/attribute.hh>

# include <functional>

namespace surface
{
  namespace gap
  {
    /// Represent the transaction state machine.
    /// Each transition from a state to another is triggered by a specific
    /// callback.
    class TransactionStateMachine
    {
    public:
      typedef std::function<void(Transaction const&)> CanceledCallback;
      typedef std::function<void(Transaction const&)> CleanCallback;
      typedef std::function<void(Transaction const&)> PrepareUploadCallback;
      typedef std::function<void(Transaction const&)> StartUploadCallback;
      typedef std::function<void(Transaction const&)> StartDownloadCallback;
      typedef std::function<void(Transaction const&)> FailedCallback;
      typedef
        std::function<bool(std::string const&, std::string const&)>
        DeviceStatusCallback;
      typedef std::function<Self const&()> SelfGetter;
      typedef std::function<Device const&()> DeviceGetter;

    private:
      ELLE_ATTRIBUTE(CanceledCallback, canceled);
      ELLE_ATTRIBUTE(FailedCallback, failed);
      ELLE_ATTRIBUTE(CleanCallback, clean);
      ELLE_ATTRIBUTE(PrepareUploadCallback, prepare_upload);
      ELLE_ATTRIBUTE(StartUploadCallback, start_upload);
      ELLE_ATTRIBUTE(StartDownloadCallback, start_download);
      ELLE_ATTRIBUTE(DeviceStatusCallback, device_status);
      ELLE_ATTRIBUTE(SelfGetter, self);
      ELLE_ATTRIBUTE(DeviceGetter, device);

    public:
      TransactionStateMachine(CanceledCallback const& canceled,
                              FailedCallback const& failed,
                              CleanCallback const& clean,
                              PrepareUploadCallback const& prepare_upload,
                              StartUploadCallback const& start_upload,
                              StartDownloadCallback const& start_download,
                              DeviceStatusCallback const& device_status,
                              SelfGetter const& self,
                              DeviceGetter const& device);

      void
      operator ()(Transaction const& transaction);
    };
  }
}

#endif
