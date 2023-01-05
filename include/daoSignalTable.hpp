#ifndef DAO_SIGNAL_TABLE_H
#define DAO_SIGNAL_TABLE_H

/** 
 *  @file   daoThreads.hpp 
 *  @brief  Class for signaling between threads
 *  @author dbarr 
 *  @date   2022-07-28 
 ***********************************************/

#include <stdint.h>
#include <cassert>
#include <unistd.h>

namespace Dao
{
    //!  SignalTable class 
    /*!
    Class to house singaling between threads
    */
   class SignalTable {
        public:
            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param max_signals the defualt number of signals the table is constructed with
             */
            SignalTable(size_t max_signals=256)
            : maxSignals(max_signals)
            {
                signal = new volatile uint64_t[max_signals];
                tracker = new volatile uint64_t[max_signals];
                SignalTableReset();
            }


            /**
             * destroy the SignalTable object and free all associated memory
             * @brief Default destructor.
             */
            virtual ~SignalTable()
            {
                SignalTableReset();
                delete[] signal;
                delete[] tracker;
            }


            /**
             * @brief default method for sending a signal. does no checking to see if anyone listening internally it just incriments a volatile signal counter
             * @param index the index of which the signal is being incrmented this must be between 0->max_signals (default 256) 
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline void SignalSend( int index )
            {
                signal[index]++;
            }


            /**
             * @brief send singals in range between indexStart->indexStart+nSignals
             * @param indexStart first signal to be sent 
             * @param nSignals number of signals to be sent
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline void SignalSendRange(int indexStart, int nSignals)
            {
                for (int i = 0; i < nSignals; i++)
                {
                    SignalSend( indexStart + i );
                }
            }


            /**
             * @brief method for sending multiple signals in array
             * @param indexArray int array containing all the signals required to be sent 
             * @param nSignals number of signals to be sent
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline void SignalSendArray(int * indexArray, int nSignals)
            {
                for (int i = 0; i < nSignals; i++)
                {
                    SignalSend( indexArray[i] );
                }
            }

            /**
             * @brief return the number of signals currently pending. Does not acknowledging any signals
             * @param index signal index to be checked 
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline uint64_t SignalsPending( int index )
            {
                return (signal[index] - tracker[index]);
            }


            /**
             * @brief Non blocking. Check if a singal is pending on index if so acknolwedge a single signal and return 1
             * @param index signal index to be checked 
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline int SignalReceive( int index )
            {
                if (SignalsPending(index) > 0)
                {
                    tracker[index]++;
                    return 1;
                }
                return 0;
            }


            /**
             * @brief Blocking. Spins on signal index until a singla arrives where the 
             * @param index signal index to be checked 
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline void SignalReceiveSpin( int index )
            {
                while(tracker[index] >= signal[index]){} // tracker should never be greater then signal but we use >= incase something bad happens
                tracker[index]++;
            }


            /**
             * @brief Blocking with timeout. Spins on index until timeout is reached or singal received
             * @param index signal index to be checked 
             * @param timeout_us timeout in us
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             * @note Not implimented
             */
            inline uint64_t SignalReceiveSpinTimeout( int signum, uint64_t timeout_us )
            {
                // TODO: impliment version using a simple timeout
                return 0;
            }

            /**
             * @brief Blocking with sleep. Spins on index signal but sleeps between checks
             * @param index signal index to be checked 
             * @param usleep us sleep time
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             * @note Not implimented
             */
            inline void SignalReceiveSleep( int index, uint64_t uSleep )
            {
                while (1)
                {
                    if (SignalReceive(index))
                    {
                        return;
                    }
                    usleep(uSleep);
                }
            }



            /**
             * @brief Blocking spin wait across range of signal index's. Will return when all signals received 
             * @param index signal index to be checked 
             * @param nSignals number of signals indexs to wait for
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline void SignalReceiveSpinRange( int index, int nSignals )
            {
                for (int i = 0; i < nSignals; i++)
                {
                    SignalReceiveSpin( index + i );
                }
            }


            /**
             * @brief Blocking spin wait across range of signal index's within array. Will return when all signals received 
             * @param indexArray int array containing all the signals required to be sent 
             * @param nSignals number of signals to be sent
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             */
            inline void SignalReceiveSpinArray( int * indexArray, int nSignals )
            {
                for (int i = 0; i < nSignals; i++)
                {
                    SignalReceiveSpin( indexArray[i] );
                }
            }


            /**
             * @brief resets the signals of index to zero
             * @param index singal index
             * @note No error checking occurs if index outside range 0->max_signals the system may segfault
             * @note this is not thread safe. Use with caution
             */
            inline void SignalReset( int index )
            {
                tracker[index] = 0;
                signal[index] = 0;
            }


            /**
             * @brief resets the signals of every signal in table
             * @note this is not thread safe. Use with caution
             */
            inline void SignalTableReset()
            {
                for(std::size_t i = 0; i < maxSignals; i++)
                {
                    SignalReset(i);
                }
            }
        
        protected:
            size_t maxSignals;
            volatile uint64_t * volatile signal;
            volatile uint64_t * volatile tracker;
    };
    // some signals to be known by all
    #define SIGNAL_THREAD_READY     0
    #define SIGNAL_LOOPSTART_ALL    1
} // close namespace Dao



#endif // DAO_SIGNAL_TABLE_H