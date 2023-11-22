Tuning the system 
=================

Introduction
------------

Optimizing your Linux installation for real-time deterministic performance involves several steps.
This document provides a comprehensive guide, explaining each step and its impact on system behavior.
Always exercise caution and test changes in a controlled environment before applying them to a production system.

Bash Script
-----------

The following Bash script performs real-time optimization tasks, addressing CPU affinity, stopping systemd processes, and additional tweaks.

.. code-block:: bash

   #!/bin/bash

   # Set CPU affinity for specific cores
   isolated_cores="0,2,4"
   echo $isolated_cores > /proc/irq/default_smp_affinity

   # Move non-kernel threads to isolated cores
   for pid in $(ps -e -o pid,cmd | awk '$2 != "kthreadd" && $2 != "migration" {print $1}'); do
       taskset -ac $isolated_cores $pid
   done

   # Additional tweaks for real-time performance
   sysctl -w kernel.sched_rt_runtime_us=-1
   sysctl -w kernel.sched_rt_period_us=1000000

   # Disable CPU frequency scaling
   for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
       echo "performance" > $cpu
   done

   # Lock memory to a specific core
   target_pid=<your_process_pid>
   mlockall -cp $target_core

Replace `<your_process_pid>` with the PID of the process you want to lock the memory for.

Explanation
~~~~~~~~~~~~

1. **CPU Affinity:**
   - **Purpose:** Isolates specific CPU cores for real-time tasks.
   - **Effect:** Reduces contention and interference from non-real-time tasks, improving determinism.

2. **Thread Affinity:**
   - **Purpose:** Moves non-kernel threads to isolated cores.
   - **Effect:** Ensures that user-level threads are executed on dedicated cores, minimizing context switching.

3. **Additional Tweaks:**
   - **Purpose:** Fine-tunes scheduler parameters for real-time performance.
   - **Effect:** Adjusts the scheduler's behavior to prioritize real-time tasks.

4. **Disable CPU Frequency Scaling:**
   - **Purpose:** Sets CPU frequency to maximum performance.
   - **Effect:** Prevents frequency scaling, ensuring consistent high performance for real-time tasks.

5. **Memory Locking:**
   - **Purpose:** Locks critical application memory in RAM.
   - **Effect:** Prevents swapping, ensuring that critical data remains in fast, non-swappable memory.

Additional Techniques
----------------------

1. **Preemption and Kernel Configurations:**
   - **Purpose:** Enhances kernel responsiveness for real-time workloads.
   - **Effect:** Reduces latency by allowing the kernel to preempt lower-priority tasks.

2. **IRQ Affinity:**
   - **Purpose:** Assigns IRQs to specific CPU cores.
   - **Effect:** Improves determinism by avoiding IRQ processing on non-isolated cores.

3. **Thread Priority:**
   - **Purpose:** Sets real-time priority for critical threads.
   - **Effect:** Ensures that high-priority threads are scheduled with minimal delay.

4. **Memory Locking:**
   - **Purpose:** Ensures critical application data is not paged out.
   - **Effect:** Prevents potential delays caused by accessing data from slower storage.

5. **RT Preempt Patches:**
   - **Purpose:** Introduces real-time preemption patches to the Linux kernel.
   - **Effect:** Improves kernel responsiveness and reduces latency for real-time tasks.

Conclusion
-----------

Real-time optimizations demand careful consideration and testing. Understand the implications of each adjustment on your system and test thoroughly before applying changes to a production environment.
