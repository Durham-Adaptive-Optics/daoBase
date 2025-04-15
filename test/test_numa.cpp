#include <gtest/gtest.h>
#include <string>
#include <daoNuma.hpp>


// TODO: Expand the testing
TEST(setAffinity, set_up) {
    // set the affinity of the current process to core 1
    // and check if it is set correctly
    #ifdef __APPLE__
    // macOS does not support setting CPU affinity in the same way as Linux
    // we ignore this test on macOS
    return;
    #elif __linux__
    int core = 1;
    Dao::Numa::SetProcAffinity(core);
    int aCore = Dao::Numa::GetProcAffinity();

    EXPECT_EQ(core, aCore);
    #elif __win32__
    // Windows does not support setting CPU affinity in the same way as Linux
    // we ignore this test on Windows
    return;
    #endif
}

TEST(allocOnNodeT, set_up) {
    // allocate a array on final node:
    int length = 100;

    int maxNode = Dao::Numa::GetMaxNode();


    float * array = Dao::Numa::AllocOnNode<float>(length, maxNode-1, 0.0);

    int numa_node = -1;
#if defined(__linux__) || defined(__linux)
    // Linux specific code
    get_mempolicy(&numa_node, NULL, 0, (void*)array, MPOL_F_NODE | MPOL_F_ADDR);
#elif (__APPLE__)
    // macOS does not support NUMA in the same way as Linux
    // we ignore this test on macOS
    numa_node = 0;
    EXPECT_EQ(maxNode-1, numa_node);
#elif __win32__
    // Windows does not support NUMA in the same way as Linux
    // we ignore this test on Windows
    numa_node = 0;
    EXPECT_EQ(maxNode-1, numa_node);
#endif
    EXPECT_EQ(maxNode-1, numa_node);
}

TEST(allocOnNode, set_up) {
    // allocate a array on final node:
    int length = 100;

    int maxNode = Dao::Numa::GetMaxNode();


    float * array = (float *) Dao::Numa::AllocOnNode(length*sizeof(float), maxNode-1,0.0);

    int numa_node = -1;
#if defined(__linux__) || defined(__linux)
    // Linux specific code
    get_mempolicy(&numa_node, NULL, 0, (void*)array, MPOL_F_NODE | MPOL_F_ADDR);
#elif (__APPLE__)
    // macOS does not support NUMA in the same way as Linux
    // we ignore this test on macOS
    numa_node = 0;
#elif __win32__
    // Windows does not support NUMA in the same way as Linux
    // we ignore this test on Windows
    numa_node = 0;
#endif

    EXPECT_EQ(maxNode-1, numa_node);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}