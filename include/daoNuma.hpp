#ifndef DAO_NUMA_HPP
#define DAO_NUMA_HPP

/** 
 *  @file   daoNumas.hpp 
 *  @brief  free functions to handle Numa
 *  @author dbarr 
 *  @date   2022-08-01 
 ***********************************************/


#if defined(__linux__)
#include <numa.h>
#include <numaif.h>
#elif defined(__APPLE__)
#include <unistd.h>
#endif 

#include <sched.h>
#include <cassert>
#include <stdlib.h> // for size_t
#include <unistd.h>

namespace Dao
{
    // has own namespace for Numa
    namespace Numa
    {
        void    SetProcAffinity( int core );
        int     GetProcAffinity();

        int     Core2Node( int core );
        int     Node2FirstCore( int node );
        void *  AllocOnNode( size_t size, int node );


        // overloading the function to add useful functionality
        template <class T>
        T *  AllocOnNode( size_t nElements, int node , T fill)
        {
            T* array = (T*) AllocOnNode(sizeof(T)*nElements, node);
            for(size_t i = 0; i < nElements; i++)
            {
                array[i] = fill;
            }
            return array;
        }

        void    Free( void * start, size_t size );

        template<class T>
        void    FreeT( T * start, size_t nElements )
        {
            Free(start, sizeof(T)*nElements);
        }

        // utilites
        int GetMaxCores();
        size_t GetMaxNode();
    };
 
}; // closes namespace Dao

#endif // DAO_THREADS_IFCE_HPP