#ifndef SURFACE_GAP_MODEL_HH
# define SURFACE_GAP_MODEL_HH

# include <das/model.hh>

# include <infinit/oracles/meta/Account.hh>
# include <infinit/oracles/meta/Device.hh>
# include <infinit/oracles/meta/ExternalAccount.hh>

namespace surface
{
  namespace gap
  {
    typedef infinit::oracles::meta::Account Account;
    typedef infinit::oracles::meta::Device Device;
    typedef infinit::oracles::meta::ExternalAccount ExternalAccount;

    class Model
    {
    public:
      Account account;
      das::IndexList<Device, elle::UUID, &Device::id> devices;
      das::IndexList<
        ExternalAccount, std::string, &ExternalAccount::id> external_accounts;
    };
  }
}

DAS_MODEL_FIELD(surface::gap::Model, account);
DAS_MODEL_FIELD(surface::gap::Model, external_accounts);
DAS_MODEL_FIELD(surface::gap::Model, devices);

namespace surface
{
  namespace gap
  {
    typedef das::Object<
      Device,
      das::Field<Device, elle::UUID, &Device::id>,
      das::Field<Device, das::Variable<std::string>, &Device::name>,
      das::Field<Device, boost::optional<std::string>, &Device::os>,
      das::Field<Device, boost::optional<std::string>, &Device::model>
      > DasDevice;
    typedef das::Collection<
      Device,
      elle::UUID,
      &Device::id,
      DasDevice
      > DasDevices;
    typedef das::Object<
      ExternalAccount,
      das::Field<ExternalAccount, std::string, &ExternalAccount::id>,
      das::Field<ExternalAccount, std::string, &ExternalAccount::type>
      > DasExternalAccount;
    typedef das::Collection<
      ExternalAccount,
      std::string,
      &ExternalAccount::id,
      DasExternalAccount
      > DasExternalAccounts;
    typedef das::Object<
      Model,
      das::Field<Model,
                 das::IndexList<Device, elle::UUID, &Device::id>,
                 &Model::devices,
                 DasDevices>,
      das::Field<Model,
                 das::IndexList<ExternalAccount, std::string, &ExternalAccount::id>,
                 &Model::external_accounts,
                 DasExternalAccounts>,
      das::Field<Model, Account, &Model::account, DasAccount>
      > DasModel;
  }
}


#endif
