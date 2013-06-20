#ifndef HOLE_MODEL_HH
# define HOLE_MODEL_HH

# include <elle/Printable.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/serialize/fwd.hh>
# include <elle/serialize/construct.hh>
# include <elle/types.hh>

namespace hole
{

  ///
  /// this class defines the model of a network i.e its implementation.
  ///
  // FIXME: should not be in hole.
  class Model:
    public elle::Printable
  {
  public:
    //
    // type
    //
    enum Type
    {
      TypeUnknown = 0,

      TypeLocal,
      TypeRemote,
      TypeSlug,
      TypeCirkle,
      TypeKool,

      Types = TypeKool,
    };

    //
    // structures
    //
    struct Descriptor
    {
      Type              type;
      elle::String      name;
    };

    //
    // constants
    //
    static const Model                  Null;
    static const Descriptor             Descriptors[Types];

    //
    // static methods
    //
    static elle::Status Convert(const elle::String&,
                                Type&);
    static elle::Status Convert(const Type,
                                elle::String&);

    //
    // constructors & destructors
    //
    Model();
    Model(const Type);
    Model(std::string const& type);
    Model(Model const& other);
    Model(Model&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Model);

    //
    // methods
    //
    elle::Status        Create(const Type);
    elle::Status        Create(const elle::String&);

    //
    // interfaces
    //

    ELLE_OPERATOR_NO_ASSIGNMENT(Model);

    elle::Boolean       operator==(const Model&) const;

    // dumpable
    elle::Status        Dump(const elle::Natural32 = 0) const;
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    //
    // attributes
    //
    ELLE_ATTRIBUTE_R(Type, type);

    ELLE_SERIALIZE_FRIEND_FOR(Model);
  };
}

#include <hole/Model.hxx>

#endif
