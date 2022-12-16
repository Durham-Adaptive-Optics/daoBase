#include <daoNuma.hpp>

void 
Dao::Numa::SetProcAffinity( int core )
{
    assert( core >= 0 );
    assert( core < Dao::Numa::GetMaxCores());

    struct bitmask * mask = numa_bitmask_alloc( 256 );
    numa_bitmask_clearall( mask );
    numa_bitmask_setbit( mask, core );
    numa_sched_setaffinity( 0, mask );
    numa_bitmask_free( mask );
}

int 
Dao::Numa::GetProcAffinity()
{
    struct bitmask * mask = numa_bitmask_alloc( 256 );
    numa_bitmask_clearall( mask );   
    numa_sched_getaffinity(0, mask);  // 0: self

    int core = -1; 
    
    for (int i = 0; i < Dao::Numa::GetMaxCores(); i++) 
    {
        if (numa_bitmask_isbitset(mask, i)) 
        {
            core = i;
            break;
        }
    }
           
    numa_bitmask_free( mask );
    return core;
}


int 
Dao::Numa::Core2Node( int core )
{
    // sanity check core.
    assert( core >= 0 );
    assert( core < Dao::Numa::GetMaxCores());

    // here we insert some 
    int node = numa_node_of_cpu(core);

    return node;
}

int 
Dao::Numa::Node2FirstCore( int node )
{
    assert(node < numa_num_configured_nodes()+1);
    int core = 0;
    struct bitmask * mask = numa_bitmask_alloc( 256 );
    numa_node_to_cpus(node, mask);
    for(int i = 0; i < Dao::Numa::GetMaxCores(); i++)
    {
        if (numa_bitmask_isbitset(mask, i)) 
        {
            core = i;
            break;
        }
    }
    numa_bitmask_free( mask );
    return core;
}

void * 
Dao::Numa::AllocOnNode( size_t size, int node )
{
    return numa_alloc_onnode(size, node);
}

void
Dao::Numa::Free( void * start, size_t size )
{
    numa_free(start,size);
}

int 
Dao::Numa::GetMaxCores()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

size_t 
Dao::Numa::GetMaxNode()
{
    return numa_num_configured_nodes();
}