#include <daoNuma.hpp>

void 
Dao::Numa::SetProcAffinity( int core )
{
    #if defined(__linux__)
    assert( core >= 0 );
    assert( core < Dao::Numa::GetMaxCores());

    struct bitmask * mask = numa_bitmask_alloc( 256 );
    numa_bitmask_clearall( mask );
    numa_bitmask_setbit( mask, core );
    numa_sched_setaffinity( 0, mask );
    numa_bitmask_free( mask );
    #else
    core = 0;
    #endif
}

int 
Dao::Numa::GetProcAffinity()
{
    int core = 0;
    #if defined(__linux__)
    struct bitmask * mask = numa_bitmask_alloc( 256 );
    numa_bitmask_clearall( mask );   
    numa_sched_getaffinity(0, mask);  // 0: self

    core = -1; 
    
    for (int i = 0; i < Dao::Numa::GetMaxCores(); i++) 
    {
        if (numa_bitmask_isbitset(mask, i)) 
        {
            core = i;
            break;
        }
    }
           
    numa_bitmask_free( mask );
    #endif
    return core;
}


int 
Dao::Numa::Core2Node( int core )
{
    int node=0;

    #if defined(__linux__)
    // sanity check core.
    assert( core >= 0 );
    assert( core < Dao::Numa::GetMaxCores());

    // here we insert some 
    node = numa_node_of_cpu(core);
    #endif
    return node;
}

int 
Dao::Numa::Node2FirstCore( int node )
{
    int core = 0;
    #if defined(__linux__)
    assert(node < numa_num_configured_nodes()+1);
    core = 0;
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
    #endif
    return core;
}

void * 
Dao::Numa::AllocOnNode( size_t size, int node )
{
    #if defined(__linux__)
    return numa_alloc_onnode(size, node);
    #else
    void * ptr = malloc(size);
    assert(ptr != NULL);
    return ptr;
    #endif
}

void
Dao::Numa::Free( void * start, size_t size )
{
    #if defined(__linux__)
    numa_free(start,size);
    #else
    free(start);
    #endif
}

int 
Dao::Numa::GetMaxCores()
{
    #if defined(__linux__)
    // return the number of cores
    return sysconf(_SC_NPROCESSORS_ONLN);
    #else
    // return the number of cores
    return 8;
    #endif
}

size_t 
Dao::Numa::GetMaxNode()
{
    #if defined(__linux__)
    return numa_num_configured_nodes();
    #else
    return 1;
    #endif
}