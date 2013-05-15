#ifndef HOLE_MODEL_HH
# define HOLE_MODEL_HH

# include <elle/types.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>

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

    //
    // methods
    //
    elle::Status        Create(const Type);
    elle::Status        Create(const elle::String&);

    //
    // interfaces
    //

    ELLE_OPERATOR_ASSIGNMENT(Model); // XXX

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
    Type                type;
  };

}

#include <hole/Model.hxx>

#endif
