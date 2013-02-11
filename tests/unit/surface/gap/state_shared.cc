#include <surface/gap/State.hh>
#include <lune/Lune.hh>
#include <boost/interprocess/managed_shared_memory.hpp>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("unit_tests.state_shared");

int
main()
{
    lune::Lune::Initialize();

    using namespace boost::interprocess;
    namespace gp = surface::gap;
    struct remover
    {
        remover() {shared_memory_object::remove("infinit_State_shm");}
        ~remover() {shared_memory_object::remove("infinit_State_shm");}
    } remove;
    gp::State S;
    std::cout << "tests done.\n";
    return (0);
}
