#ifndef NUCLEUS_DERIVABLE_HXX
# define NUCLEUS_DERIVABLE_HXX

# include <stdexcept>

# include <elle/serialize/Serializer.hh>
# include <elle/utility/Factory.hh>
# include <elle/log.hh>

# include <nucleus/proton/Block.hh>

ELLE_SERIALIZE_NO_FORMAT(nucleus::Derivable);

ELLE_SERIALIZE_SPLIT(nucleus::Derivable);

ELLE_SERIALIZE_SPLIT_LOAD(nucleus::Derivable, archive, value, version)
{
  ELLE_LOG_COMPONENT("nucleus.Derivable");

  enforce(value.kind == nucleus::Derivable::Kind::output);
  enforce(version == 0);
  archive >> value._component;

  ELLE_DEBUG("extracted component: %s", value._component);

  if (value._dynamic_construct)
    {
      enforce(value._block == nullptr);

      auto const& factory = nucleus::proton::block::factory<>();

      value._block = factory.allocate<nucleus::proton::Block>(value._component);
    }
  enforce(value._block != nullptr);
  typedef typename elle::serialize::SerializableFor<Archive>::Type interface_t;
  enforce(dynamic_cast<interface_t*>(value._block) != nullptr);
  static_cast<interface_t&>(*value._block).deserialize(archive);
}

ELLE_SERIALIZE_SPLIT_SAVE(nucleus::Derivable, archive, value, version)
{
  ELLE_LOG_COMPONENT("nucleus.Derivable");

  enforce(version == 0);
  enforce(value._block != nullptr);

  ELLE_DEBUG("serialize component: %s", value._component);
  archive << value._component;
  archive << *value._block;
}

#endif
