#ifndef SURFACE_GAP_MODEL_HH
# define SURFACE_GAP_MODEL_HH

# include <das/model.hh>

# include <surface/gap/Device.hh>

namespace surface
{
  namespace gap
  {
    class Model
    {
    public:
      std::vector<Device> devices;
    };
  }
}

DAS_MODEL_FIELD(surface::gap::Model, devices);

DAS_MODEL_FIELD(surface::gap::Device, id);
DAS_MODEL_FIELD(surface::gap::Device, name);
DAS_MODEL_FIELD(surface::gap::Device, os);

namespace surface
{
  namespace gap
  {
    typedef das::Object<
      Device,
      das::Field<Device, elle::UUID, &Device::id>,
      das::Field<Device, das::Variable<std::string>, &Device::name>,
      das::Field<Device, std::string, &Device::os>
      > DasDevice;
    typedef das::Collection<
      Device,
      elle::UUID,
      &Device::id,
      DasDevice
      > DasDevices;
    typedef das::Object<
      Model,
      das::Field<Model, std::vector<Device>, &Model::devices, DasDevices>
      > DasModel;
  }
}


#endif
