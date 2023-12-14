#include <cuda_runtime.h>
#include "dao.h"

//__global__ void imageProcessingKernel(IMAGE *deviceImage) {
//    // GPU kernel to process the image
//    // ...
//}

int main() {
    IMAGE hostImage;
    IMAGE *deviceImage;

    // Allocate memory for IMAGE on GPU
    cudaMalloc((void**)&deviceImage, sizeof(IMAGE));

    // Copy IMAGE data from host to GPU
    cudaMemcpy(deviceImage, &hostImage, sizeof(IMAGE), cudaMemcpyHostToDevice);

//    // Launch kernel to process the image
//    imageProcessingKernel<<<1, 1>>>(deviceImage);

    // Copy back the results from GPU to host
    cudaMemcpy(&hostImage, deviceImage, sizeof(IMAGE), cudaMemcpyDeviceToHost);

    // Free GPU memory
    cudaFree(deviceImage);

    return 0;
}
