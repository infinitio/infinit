#include <elle/types.hh>
#include <elle/assert.hh>
#include <elle/Exception.hh>
#include <elle/finally.hh>
#include <elle/format/hexadecimal.hh>

#include <reactor/exception.hh>

#include <cryptography/Digest.hh>
#include <cryptography/oneway.hh>
#include <cryptography/random.hh>

#include <nucleus/proton/Porcupine.hh>
#include <nucleus/proton/Nest.hh>
#include <nucleus/proton/Door.hh>
#include <nucleus/proton/Limits.hh>
#include <nucleus/proton/Statistics.hh>
#include <nucleus/neutron/Catalog.hh>
#include <nucleus/neutron/Entry.hh>
#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Data.hh>
#include <nucleus/neutron/Attributes.hh>

#include <etoile/Etoile.hh>
#include <etoile/gear/Transcript.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/nest/Nest.hh>

#include <lune/Lune.hh>

#include <hole/Hole.hh>
#include <hole/storage/Memory.hh>
#include <hole/implementations/local/Implementation.hh>

#include <reactor/scheduler.hh>
#include <Infinit.hh>

// XXX[temporary: for cryptography]
using namespace infinit;

ELLE_LOG_COMPONENT("infinit.tests.nucleus.proton.Porcupine");

// To define in order to make the checks stronger and so as to
// detect inconsistencies early on.
#undef PORCUPINE_THOROUGH_CHECK

// To define to test the serialization mechanism with porcupines.
#define PORCUPINE_SERIALIZE_TEST

// To define to dump the porcupine's statistcs.
#undef PORCUPINE_STATISTICS

// Define the length (in bits) of the secret key for encrypting content blocks.
#define PORCUPINE_SECRET_LENGTH 256

std::unique_ptr<hole::storage::Memory> _storage(nullptr);
std::unique_ptr<nucleus::proton::Network> _network(nullptr);
std::unique_ptr<cryptography::KeyPair> _user(nullptr);

//
// ---------- Catalog ---------------------------------------------------------
//

std::vector<elle::String>
test_porcupine_catalog_prepare(elle::Natural32 const n)
{
  std::vector<elle::String> vector(n);

  for (elle::Natural32 i = 0; i < n; i++)
    {
      cryptography::Digest digest{
        cryptography::oneway::hash(i, cryptography::oneway::Algorithm::sha1)};

      elle::String string = elle::format::hexadecimal::encode(digest.buffer());

      vector[i] = string;
    }

  return (vector);
}

void
test_porcupine_catalog_vector_dump(std::vector<elle::String> const& vector)
{
  for (auto& s: vector)
    std::cout << s << std::endl;
}

void
test_porcupine_catalog_add(
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>& porcupine,
  std::vector<elle::String>& vector)
{
  ELLE_TRACE_FUNCTION(porcupine, vector);

  for (elle::Natural32 i = 0; i < vector.size(); i++)
    {
      ELLE_TRACE_SCOPE("[%s] add(%s)", i, vector[i]);

      nucleus::proton::Door<nucleus::neutron::Catalog> catalog{
        porcupine.lookup(vector[i])};

      nucleus::neutron::Entry* entry{
        new nucleus::neutron::Entry(vector[i],
                                    nucleus::proton::Address::null())};

      catalog.open();

      catalog().insert(entry);

      catalog.close();

      porcupine.update(vector[i]);

#ifdef PORCUPINE_THOROUGH_CHECK
      porcupine.check(
        nucleus::proton::flags::recursive |
        nucleus::proton::flags::key |
        nucleus::proton::flags::capacity |
        nucleus::proton::flags::footprint |
        nucleus::proton::flags::state);
#endif
    }

  ELLE_ASSERT(porcupine.size() == vector.size());
  ELLE_ASSERT(porcupine.state() == nucleus::proton::State::dirty);

  porcupine.check(
    nucleus::proton::flags::recursive |
    nucleus::proton::flags::key |
    nucleus::proton::flags::capacity |
    nucleus::proton::flags::footprint |
    nucleus::proton::flags::state);
}

void
test_porcupine_catalog_lookup(
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>& porcupine,
  std::vector<elle::String>& vector)
{
  ELLE_TRACE_FUNCTION(porcupine, vector);

  ELLE_ASSERT(vector.size() == porcupine.size());

  for (elle::Natural32 i = 0; i < vector.size(); i++)
    {
      ELLE_TRACE_SCOPE("[%s] lookup(%s)", i, vector[i]);

      nucleus::proton::Door<nucleus::neutron::Catalog> catalog{
        porcupine.lookup(vector[i])};

      catalog.open();

      ELLE_ASSERT(catalog().exist(vector[i]) == true);

      ELLE_ASSERT(porcupine.exist(vector[i]) == true);

      catalog.close();

#ifdef PORCUPINE_THOROUGH_CHECK
      porcupine.check(
        nucleus::proton::flags::recursive |
        nucleus::proton::flags::key |
        nucleus::proton::flags::capacity |
        nucleus::proton::flags::footprint |
        nucleus::proton::flags::state);
#endif
    }

  porcupine.check(
    nucleus::proton::flags::recursive |
    nucleus::proton::flags::key |
    nucleus::proton::flags::capacity |
    nucleus::proton::flags::footprint |
    nucleus::proton::flags::state);
}

nucleus::proton::Radix
test_porcupine_catalog_seal(
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>& porcupine,
  cryptography::SecretKey& secret)
{
  ELLE_TRACE_FUNCTION(porcupine, secret);

  nucleus::proton::Radix radix{porcupine.seal(secret)};

  porcupine.check(
    nucleus::proton::flags::all);

  return (radix);
}

void
test_porcupine_catalog_seek(
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>& porcupine,
  std::vector<elle::String>& vector)
{
  ELLE_TRACE_FUNCTION(porcupine, vector);

  ELLE_ASSERT(vector.size() == porcupine.size());

  std::vector<elle::String> w(vector.size());

  for (elle::Natural32 i = 0; i < vector.size();)
    {
      ELLE_TRACE_SCOPE("[%s] seek(%s)", i, i);

      auto pair = porcupine.seek(i);
      auto& catalog = pair.first;
      auto& base = pair.second;

      catalog.open();

      nucleus::neutron::Range<nucleus::neutron::Entry> range{
        catalog().consult(i - base,
                          catalog().capacity() - (i - base))};

      catalog.close();

      for (auto& entry: range)
        w[i++] = entry->name();

#ifdef PORCUPINE_THOROUGH_CHECK
      porcupine.check(
        nucleus::proton::flags::recursive |
        nucleus::proton::flags::key |
        nucleus::proton::flags::capacity |
        nucleus::proton::flags::footprint |
        nucleus::proton::flags::state);
#endif
    }

  std::vector<elle::String> s(vector);
  sort(s.begin(), s.end());

  ELLE_ASSERT(s == w);

  porcupine.check(
    nucleus::proton::flags::all);
}

nucleus::proton::Porcupine<nucleus::neutron::Catalog>*
test_porcupine_catalog_serialize(
  nucleus::proton::Radix radix1,
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>& porcupine1,
  cryptography::SecretKey& secret,
  std::vector<elle::String>& vector)
{
  etoile::nest::Nest* nest1 =
    static_cast<etoile::nest::Nest*>(&porcupine1.nest());

  ELLE_TRACE_FUNCTION(radix1, porcupine1, secret, vector);

  etoile::gear::Transcript transcript = nest1->transcribe();

  for (auto action: transcript)
    action->apply<etoile::depot::Depot>();

  elle::Buffer buffer;

  buffer.writer() << radix1;
  nucleus::proton::Radix radix2;
  buffer.reader() >> radix2;

  etoile::nest::Nest* nest2 =
    new etoile::nest::Nest(PORCUPINE_SECRET_LENGTH,
                           porcupine1.nest().limits(),
                           *_network,
                           _user->K(),
                           nest1->threshold());
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>* porcupine2 =
    new nucleus::proton::Porcupine<nucleus::neutron::Catalog>(radix2,
                                                              secret,
                                                              *nest2);

  // This first lookup phase is going to be slower than when the
  // porcupine has been created because blocks needed to be fetched
  // from the storage layer.
  test_porcupine_catalog_lookup(*porcupine2, vector);

  test_porcupine_catalog_seek(*porcupine2, vector);

  porcupine2->check(
    nucleus::proton::flags::all);

  return (porcupine2);
}

void
test_porcupine_catalog_remove(
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>& porcupine,
  std::vector<elle::String>& vector,
  elle::Natural32 const index,
  elle::Natural32 const size)
{
  ELLE_TRACE_FUNCTION(porcupine, vector, index, size);

  ELLE_ASSERT(vector.size() == porcupine.size());

  for (elle::Natural32 i = index; i < (index + size); i++)
    {
      ELLE_TRACE_SCOPE("[%s] remove(%s)", i, vector[i]);

      nucleus::proton::Door<nucleus::neutron::Catalog> catalog{
        porcupine.lookup(vector[i])};

      catalog.open();

      catalog().erase(vector[i]);

      catalog.close();

      porcupine.update(vector[i]);

#ifdef PORCUPINE_THOROUGH_CHECK
      porcupine.check(
        nucleus::proton::flags::recursive |
        nucleus::proton::flags::key |
        nucleus::proton::flags::capacity |
        nucleus::proton::flags::footprint |
        nucleus::proton::flags::state);
#endif
    }

  vector.erase(vector.begin() + index, vector.begin() + index + size);

  porcupine.check(
    nucleus::proton::flags::recursive |
    nucleus::proton::flags::key |
    nucleus::proton::flags::capacity |
    nucleus::proton::flags::footprint |
    nucleus::proton::flags::state);

  ELLE_ASSERT(porcupine.state() == nucleus::proton::State::dirty);
}

void
test_porcupine_catalog()
{
  ELLE_TRACE_FUNCTION("");

  elle::Natural32 const N = 1283;

  std::vector<elle::String> vector = test_porcupine_catalog_prepare(N);

  // XXX[provide a path to where blocks should be stored i.e another
  //     hole storage]
  etoile::nest::Nest nest1{
    PORCUPINE_SECRET_LENGTH,
    nucleus::proton::Limits(nucleus::proton::limits::Porcupine{},
                            nucleus::proton::limits::Node{1024, 0.5, 0.2},
                            nucleus::proton::limits::Node{1024, 0.5, 0.2}),
      *_network,
      _user->K(),
      6315};

  nucleus::proton::Porcupine<nucleus::neutron::Catalog>* porcupine1 =
    new nucleus::proton::Porcupine<nucleus::neutron::Catalog>(nest1);

  test_porcupine_catalog_add(*porcupine1, vector);

  ELLE_ASSERT(porcupine1->strategy() == nucleus::proton::Strategy::tree);
  ELLE_ASSERT(porcupine1->tree().height() >= 1);
  ELLE_ASSERT(porcupine1->tree().height() <= 10);

  test_porcupine_catalog_lookup(*porcupine1, vector);

#ifdef PORCUPINE_STATISTICS
  nucleus::proton::Statistics stats =
    porcupine1->statistics();
  stats.Dump();
#endif

  cryptography::SecretKey secret1 =
    cryptography::SecretKey::generate(cryptography::cipher::Algorithm::aes256,
                                      PORCUPINE_SECRET_LENGTH);

  nucleus::proton::Radix radix1{
    test_porcupine_catalog_seal(*porcupine1, secret1)};

  test_porcupine_catalog_seek(*porcupine1, vector);

#ifdef PORCUPINE_SERIALIZE_TEST
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>* porcupine2 =
    test_porcupine_catalog_serialize(radix1, *porcupine1, secret1, vector);

  test_porcupine_catalog_remove(*porcupine2, vector, N / 3, N / 3);

  test_porcupine_catalog_lookup(*porcupine2, vector);

  cryptography::SecretKey secret2 =
    cryptography::SecretKey::generate(cryptography::cipher::Algorithm::aes256,
                                      PORCUPINE_SECRET_LENGTH);

  nucleus::proton::Radix radix2{
    test_porcupine_catalog_seal(*porcupine2, secret2)};

  nucleus::proton::Porcupine<nucleus::neutron::Catalog>* porcupine3 =
    test_porcupine_catalog_serialize(radix2, *porcupine2, secret2, vector);

  delete porcupine3;

  delete porcupine2;

  etoile::nest::Nest nest4(PORCUPINE_SECRET_LENGTH,
                           nest1.limits(),
                           *_network,
                           _user->K(),
                           nest1.threshold());
  nucleus::proton::Porcupine<nucleus::neutron::Catalog>* porcupine4 =
    new nucleus::proton::Porcupine<nucleus::neutron::Catalog>(radix2,
                                                              secret2,
                                                              nest4);

  delete porcupine1;

  test_porcupine_catalog_lookup(*porcupine4, vector);

  test_porcupine_catalog_remove(*porcupine4, vector, 0, vector.size());

  etoile::gear::Transcript transcript = nest4.transcribe();

  for (auto action: transcript)
    action->apply<etoile::depot::Depot>();

  ELLE_ASSERT(porcupine4->empty() == true);
  ELLE_ASSERT(_storage->empty() == true);

  delete porcupine4;
#endif
}

//
// ---------- Data ------------------------------------------------------------
//

void
test_porcupine_data_write(
  nucleus::proton::Porcupine<nucleus::neutron::Data>& porcupine)
{
  ELLE_TRACE_FUNCTION(porcupine);

  elle::Natural32 const N = 12360;

  elle::Buffer input = cryptography::random::generate<elle::Buffer>(N);

  nucleus::proton::Door<nucleus::neutron::Data> data{
    porcupine.lookup(porcupine.size())};

  data.open();

  data().write(0, elle::WeakBuffer{input});

  data.close();

  porcupine.update(0);

  porcupine.check(
    nucleus::proton::flags::recursive |
    nucleus::proton::flags::key |
    nucleus::proton::flags::capacity |
    nucleus::proton::flags::footprint |
    nucleus::proton::flags::state);

  auto _index = static_cast<nucleus::proton::Capacity>(0);
  auto _size = N;

  elle::Buffer output;

  while (_size > 0)
    {
      auto pair = porcupine.seek(_index);
      auto& door = pair.first;
      auto& base = pair.second;

      door.open();

      auto start = _index - base;
      auto length = _size > (door().size() - start) ?
        (door().size() - start) : _size;

      ELLE_ASSERT(start == 0);
      ELLE_ASSERT(length != 0);
      ELLE_ASSERT(length == door().size());

      door().read(start, length, output);

      door.close();

      _index += length;
      _size -= length;
    }

  ELLE_ASSERT(input == output);
}

void
test_porcupine_data_resize(
  nucleus::proton::Porcupine<nucleus::neutron::Data>& porcupine)
{
  ELLE_TRACE_FUNCTION(porcupine);

  nucleus::neutron::Size size = 1234;

  nucleus::proton::Door<nucleus::neutron::Data> base{
    porcupine.lookup(size)};

  while (true)
    {
#ifdef PORCUPINE_THOROUGH_CHECK
      porcupine.check(
        nucleus::proton::flags::recursive |
        nucleus::proton::flags::key |
        nucleus::proton::flags::capacity |
        nucleus::proton::flags::footprint |
        nucleus::proton::flags::state);
#endif

      nucleus::proton::Door<nucleus::neutron::Data> end{
        porcupine.tail()};

      if (end == base)
        break;
      else
        {
          end.open();

          nucleus::neutron::Offset key = end().mayor();

          end.close();

          porcupine.tree().remove(key);
        }
    }

  base.open();

  base().adjust(size - base().offset());

  base.close();

  porcupine.update(size);

  porcupine.check(
    nucleus::proton::flags::recursive |
    nucleus::proton::flags::key |
    nucleus::proton::flags::capacity |
    nucleus::proton::flags::footprint |
    nucleus::proton::flags::state);
}

void
test_porcupine_data()
{
  ELLE_TRACE_FUNCTION("");

  // XXX[provide a path to where blocks should be stored i.e another
  //     hole storage]
  etoile::nest::Nest nest1{
    PORCUPINE_SECRET_LENGTH,
    nucleus::proton::Limits(nucleus::proton::limits::Porcupine{},
                            nucleus::proton::limits::Node{1024, 1.0, 0.0},
                            nucleus::proton::limits::Node{1024, 1.0, 0.0}),
      *_network,
      _user->K(),
      3176};
  nucleus::proton::Porcupine<nucleus::neutron::Data>* porcupine =
    new nucleus::proton::Porcupine<nucleus::neutron::Data>(nest1);

  test_porcupine_data_write(*porcupine);
  test_porcupine_data_resize(*porcupine);

  delete porcupine;
}

//
// ---------- Attributes ------------------------------------------------------
//

void
test_porcupine_attributes()
{
  ELLE_TRACE_FUNCTION("");

  // XXX[provide a path to where blocks should be stored i.e another
  //     hole storage]
  etoile::nest::Nest nest1{
    PORCUPINE_SECRET_LENGTH,
    nucleus::proton::Limits(nucleus::proton::limits::Porcupine{},
                            nucleus::proton::limits::Node{1024, 1.0, 0.0},
                            nucleus::proton::limits::Node{1024, 1.0, 0.0}),
      *_network,
      _user->K(),
      1048576};

  nucleus::proton::Porcupine<nucleus::neutron::Attributes>* porcupine =
    new nucleus::proton::Porcupine<nucleus::neutron::Attributes>(nest1);

  std::vector<elle::String> const vector({"suce", "avale", "leche",
                                          "ingurgite"});

  for (auto& name: vector)
    {
      nucleus::proton::Door<nucleus::neutron::Attributes> door{
        porcupine->lookup(name)};

      door.open();

      nucleus::neutron::Trait* trait =
        new nucleus::neutron::Trait(name, name);

      door().insert(trait);

      door.close();

      porcupine->update(name);
    }

  porcupine->check(nucleus::proton::flags::all);

  delete porcupine;
}

//
// ---------- Main ------------------------------------------------------------
//

int
Main(elle::Natural32,
     elle::Character* argv[],
     reactor::Scheduler& sched)
{
  try
    {
      // XXX
      Infinit::Network = "test";
      lune::Lune::Initialize();
      Infinit::Initialize();
      // XXX

      _network.reset(new nucleus::proton::Network(Infinit::Network));
      _user.reset(
        new cryptography::KeyPair(
          cryptography::KeyPair::generate(
            cryptography::Cryptosystem::rsa,
            1024)));

#ifdef PORCUPINE_SERIALIZE_TEST
      cryptography::KeyPair pair_authority{
        cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024)};
      elle::Authority authority(pair_authority);

      elle::Passport passport(elle::String{"node"},
                              elle::String{"me"},
                              _user->K(),
                              authority);

      _storage.reset(new hole::storage::Memory(*_network));

      hole::Hole* hole =
        new hole::implementations::local::Implementation(
          *_storage, passport, authority);

      ELLE_FINALLY_ACTION_DELETE(hole);

      etoile::depot::hole(hole);
      etoile::Etoile::Initialize();

      hole->join();
#endif

      test_porcupine_catalog();
      test_porcupine_data();
      test_porcupine_attributes();

#ifdef PORCUPINE_SERIALIZE_TEST
      etoile::Etoile::Clean();

      hole->leave();

      ELLE_FINALLY_ABORT(hole);

      delete hole;
#endif

      // XXX
      Infinit::Clean();
      lune::Lune::Clean();
      // XXX

      std::cout << "tests done.\n";
    }
  catch (reactor::Exception const& e)
    {
      std::cerr << argv[0] << ": fatal error: " << e << std::endl;
      goto _error;
    }
  catch (std::exception const& e)
    {
      std::cerr << argv[0] << ": fatal error: " << e.what() << std::endl;
      goto _error;
    }
  catch (...)
    {
      std::cerr << argv[0] << ": unknown exception" << std::endl;
      goto _error;
    }

  sched.terminate();
  return (0);

 _error:
  sched.terminate();
  return (1);
}

int
main(int argc,
     char* argv[])
{
  reactor::Scheduler sched{};

  reactor::VThread<int> main{sched, "main", std::bind(&Main, argc, argv, std::ref(sched))};

  sched.run();

  return main.result();
}
