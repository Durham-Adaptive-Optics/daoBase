#include <gtest/gtest.h>
#include <string>
#include <daoNuma.hpp>


// TODO: Expand the testing
TEST(setNusetAffinity, set_up) {
    int core = 1;
    Dao::Numa::SetProcAffinity(core);
    int aCore = Dao::Numa::GetProcAffinity();

    EXPECT_EQ(core, aCore);

}

TEST(allocOnNodeT, set_up) {
    // allocate a array on final node:
    int length = 100;

    int maxNode = Dao::Numa::GetMaxNode();


    float * array = Dao::Numa::AllocOnNode<float>(length, maxNode-1, 0.0);

    int numa_node = -1;
    get_mempolicy(&numa_node, NULL, 0, (void*)array, MPOL_F_NODE | MPOL_F_ADDR);

    EXPECT_EQ(maxNode-1, numa_node);
}

TEST(allocOnNode, set_up) {
    // allocate a array on final node:
    int length = 100;

    int maxNode = Dao::Numa::GetMaxNode();


    float * array = (float *) Dao::Numa::AllocOnNode(length*sizeof(float), maxNode-1,0.0);

    int numa_node = -1;
    get_mempolicy(&numa_node, NULL, 0, (void*)array, MPOL_F_NODE | MPOL_F_ADDR);

    EXPECT_EQ(maxNode-1, numa_node);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}