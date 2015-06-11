#ifndef SURFACE_GAP_MODEL_HH
# define SURFACE_GAP_MODEL_HH

# include <das/model.hh>

# include <infinit/oracles/meta/Account.hh>
# include <infinit/oracles/meta/Device.hh>

namespace surface
{
  namespace gap
  {
    typedef infinit::oracles::meta::Account Account;
    typedef infinit::oracles::meta::Device Device;

    class Model
    {
    public:
      das::IndexList<Account, std::string, &Account::id> accounts;
      das::IndexList<Device, elle::UUID, &Device::id> devices;
    };
  }
}

DAS_MODEL_FIELD(surface::gap::Model, accounts);
DAS_MODEL_FIELD(surface::gap::Model, devices);

namespace surface
{
  namespace gap
  {
    typedef das::Object<
      Account,
      das::Field<Account, std::string, &Account::id>,
      das::Field<Account, std::string, &Account::type>
      > DasAccount;
    typedef das::Collection<
      Account,
      std::string,
      &Account::id,
      DasAccount
      > DasAccounts;
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
      Model,
      das::Field<Model,
                 das::IndexList<Device, elle::UUID, &Device::id>,
                 &Model::devices,
                 DasDevices>,
      das::Field<Model,
                 das::IndexList<Account, std::string, &Account::id>,
                 &Model::accounts,
                 DasAccounts>
      > DasModel;
  }
}


#endif
