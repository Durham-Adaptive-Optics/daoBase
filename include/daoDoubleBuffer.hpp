/**
 * @file    daoDoubleBuffer.h
 * @brief   Double buffer for   definition
 *
 *
 * @author  D. Barr
 * @date    03 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_DOUBLE_BUFFER_HPP
#define DAO_DOUBLE_BUFFER_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <daoNuma.hpp>

namespace Dao
{
    template <class T> class DoubleBuffer {
        public:
            DoubleBuffer(size_t numberOfElements, int alloc_now_node = -1, T fillvalue = 0 )
            : m_n_element(numberOfElements)
            , m_active_index(-1)
            , m_active_buffer(0)
            , m_node(alloc_now_node)
            , m_dirty(true)
            , m_target_frame(0)
            , m_target_frame_set(false)
            {
                assert(m_n_element >= 1);

                if (m_node >= 0)
                {
                    AllocOnNode();
                    for (size_t i = 0; i < m_n_element; i++)
                    {
                        m_buffer[0][i] = fillvalue;
                        m_buffer[1][i] = fillvalue;
                    }
                }
                else
                {
                    m_buffer[0] = (T *) malloc( sizeof(T) * m_n_element );
                    m_buffer[1] = (T *) malloc( sizeof(T) * m_n_element );

                    // set active buffer to 0
                    SetActiveBuffer(0);
                    for (size_t i = 0; i < m_n_element; i++)
                    {
                        m_buffer[0][i] = fillvalue;
                        m_buffer[1][i] = fillvalue;
                    }
                }
            };

            ~DoubleBuffer()
            {
                if (m_active_buffer != 0)
                {
                    Numa::Free(m_buffer[0], sizeof(T) * m_n_element);
                    Numa::Free(m_buffer[1], sizeof(T) * m_n_element);
                }
                else
                {
                    free(m_buffer[0]);
                    free(m_buffer[1]);
                }
            };

            int GetActiveIndex()
            {
                assert( m_active_index != -1 );
                return m_active_index;
            };

            int GetInctiveIndex()
            {
                assert( m_active_index != -1 );
                if (m_active_index == 0)
                    return 1;
                else
                    return 0;
            };

            void SetActiveBuffer( int index )
            {
                assert( ((index == 0) || (index == 1)) );
                m_active_index = index;
                m_active_buffer = m_buffer[index];
            };

            int GetNode()
            {
                return m_node;
            }

            void SwapBuffers()
            {
                SetActiveBuffer( GetInctiveIndex() );
                m_dirty = false;
                m_target_frame_set = false;
                m_target_frame = 0;
            };

            // allocate both buffers on node
            void AllocOnNode()
            {
                assert( m_active_buffer == 0 );

                m_buffer[0] = (T *) Numa::AllocOnNode( sizeof(T) * m_n_element, m_node );
                m_buffer[1] = (T *) Numa::AllocOnNode( sizeof(T) * m_n_element, m_node );

                // set active buffer to 0
                SetActiveBuffer(0);
            };

            // fast check: Dangerous as no checks are carried out
            T * Active()
            {
                return m_active_buffer;
            };

            T * Active(uint64_t &frame)
            {
                if(m_target_frame_set && frame >= m_target_frame)
                {
                    SwapBuffers();
                }
                return m_active_buffer;
            };

            // get passive mpa fast no checkms
            T * Passive()
            {
                return m_buffer[GetInctiveIndex()];
            }
            
            // very dangerous as no checks on input data performed
            // very easy to cause a seg faukt
            void CopyAndSwap(T * data)
            {
                T * passive_buffer = (T*) Passive();
                memcpy(passive_buffer, data, sizeof(T)*m_n_element);
                SwapBuffers();
            }

            void CopyIn(T * data, uint64_t frame = 0)
            {
                if(frame != 0)
                {
                    m_target_frame_set = true;
                    m_target_frame = frame;
                }

                T * passive_buffer = (T*) Passive();
                memcpy(passive_buffer, data, sizeof(T)*m_n_element);

                m_dirty = true;

            }

            size_t Size()
            {
                return m_n_element;
            };

            void SetDirty(){m_dirty = true;};
            bool GetDirty(){return m_dirty;};

        private:
            size_t m_n_element;     // number of elements in the buffer
            int m_active_index;     // index to active bank, 0 or 1; -1 means not alloc
            T * m_active_buffer;    // pointer to presently active bank, null means not alloc
            T * m_buffer[2];        // the banks
            int m_node;
            bool m_dirty;           // if inactive buffer has been updated but the buffered not switched
            uint64_t m_target_frame;
            bool m_target_frame_set;
    };
} // closing namespace Dao

#endif /* DAO_DOUBLE_BUFFER_HPP */
