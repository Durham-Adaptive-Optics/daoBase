#include "mex.h"
#include "dao.h"
#include <string.h> 

// Function to initialize an IMAGE structure and return a pointer to it
void shm_init(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    if (nrhs < 1 || nrhs > 2 || nlhs != 1)
    {
        mexErrMsgIdAndTxt("MATLAB:myMexFunction:invalidNumArgs",
                          "Invalid number of input or output arguments.");
    }

    // Parse input arguments
    const char *name = mxArrayToString(prhs[0]);

    IMAGE *newImage = (IMAGE *)malloc(sizeof(IMAGE));
    // Check if the optional input argument is provided
    if (nrhs == 2 && !mxIsEmpty(prhs[1]) && mxIsNumeric(prhs[1]))
    {
        // The optional argument is a non-empty numeric matrix
        daoInfo("%s will be created or overwritten\n", name);

        const mxArray *dataMatrix = prhs[1];
        int numRows = mxGetM(dataMatrix);
        int numCols = mxGetN(dataMatrix);

        // Determine the data type
        mxClassID dataType = mxGetClassID(dataMatrix);
        const char *dataTypeName = mxGetClassName(dataMatrix);

        daoInfo("Matrix type: %s\n", dataTypeName);
        daoInfo("Matrix size: %d x %d\n", numRows, numCols);
        // Create size array, using 2D of 1x1
        uint32_t size[2];
        size[0] = numRows;
        size[1] = numCols;
        int_fast8_t result;
        // Create a new IMAGE structure
        // Check the data type
        if (dataType == mxUINT8_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_UINT8, 1, 0);
        }
        else if (dataType == mxINT8_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_INT8, 1, 0);
        }
        else if (dataType == mxUINT16_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_UINT16, 1, 0);
        }
        else if (dataType == mxINT16_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_INT16, 1, 0);
        }
        else if (dataType == mxUINT32_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_UINT32, 1, 0);
        }
        else if (dataType == mxINT64_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_INT64, 1, 0);
        }
        else if (dataType == mxSINGLE_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_FLOAT, 1, 0);
        }
        else if (dataType == mxDOUBLE_CLASS)
        {
            result = daoShmImageCreate(newImage, name, 2, size, _DATATYPE_DOUBLE, 1, 0);
        }
        else
        {
            mexErrMsgIdAndTxt("MATLAB:shm_init:invalidNumArgs",
                              "Data type not supported.");
        }

        //if (result != DAO_SUCCESS)
        //{
        //    mexErrMsgIdAndTxt("MATLAB:shm_init:daoError",
        //                      "daoShmImageCreate failed.");
        //}
        //else
        //{
        //    daoShmImage2Shm(mxGetData(dataMatrix), numRows*numCols, &newImage[0]);
        //}
    }
    else
    {
        // The optional argument is not provided or is empty
        daoInfo("loading existing %s\n", name);

        // Create a new IMAGE structure
        if (newImage == NULL)
        {
            mexErrMsgIdAndTxt("MATLAB:shm_init:memoryAllocationError",
                              "Failed to allocate memory for IMAGE structure.");
        }

        // Call the daoShmShm2Img function to initialize the IMAGE structure
        int_fast8_t result = daoShmShm2Img(name, newImage);

        if (result != DAO_SUCCESS)
        {
            mexErrMsgIdAndTxt("MATLAB:shm_init:daoError",
                              "daoShmShm2Img failed.");
        }
    }

    // Return a pointer to the IMAGE structure to MATLAB
    plhs[0] = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
    uint64_t *ptr = (uint64_t *)mxGetData(plhs[0]);
    *ptr = (uint64_t)newImage;
}

// Function to access and work with a selected IMAGE structure
void get_data(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    if (nrhs < 1 || nrhs > 3 || nlhs != 1)
    {
        mexErrMsgIdAndTxt("MATLAB:get_data:invalidNumArgs",
                          "Invalid number of input or output arguments.");
    }

    // Parse the input argument to get the pointer to the IMAGE structure
    uint64_t *ptr = (uint64_t *)mxGetData(prhs[0]);
    IMAGE *selectedImage = (IMAGE *)(*ptr);

    // Check if a valid IMAGE is selected
    if (selectedImage == NULL)
    {
        mexErrMsgIdAndTxt("MATLAB:get_data:noImageSelected",
                          "No IMAGE structure is currently selected. Call 'initImage' first.");
    }

    // Initialize the boolean parameter with a default value (false)
    bool waitForSemaphore = false;

    // Initialize the integer parameter with a default value (1)
    int semNb = 1;

    // Check if an optional boolean parameter is provided
    if (nrhs >= 2 && !mxIsEmpty(prhs[1]) && mxIsLogicalScalar(prhs[1]))
    {
        mxLogical *logicalArray = mxGetLogicals(prhs[1]);
        waitForSemaphore = (logicalArray[0] == 1); // Convert the logical value to a boolean
    }

    // Check if an optional integer parameter is provided
    if (nrhs >= 3 && !mxIsEmpty(prhs[2]) && mxIsNumeric(prhs[2]) && mxIsScalar(prhs[2]))
    {
        double *numericArray = mxGetPr(prhs[2]);
        semNb = (int)numericArray[0]; // Convert the numeric value to an integer
    }

    if (waitForSemaphore == true)
    {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // 1 second timeout
        if (sem_timedwait(selectedImage[0].semptr[semNb], &timeout) == -1)
        {
            daoInfo("Time out (1s) waiting for new data in the SHM, using what is currently in it\n");
        }
    }
    // Determine the size of the UI8 array
    uint32_t numRows = selectedImage[0].md[0].size[0];
    uint32_t numCols = selectedImage[0].md[0].size[1];

    // Create a MATLAB matrix of the appropriate size and type (uint8)
    mxArray *dataMatrix;

    // WEIRD... signed type between libdao and mex seemd swapped!
    // 
    // _DATATYPE_UINT8 <-> mxINT8_CLASS
    // _DATATYPE_INT8 <-> mxUINT8_CLASS
    // _DATATYPE_UINT16 <-> mxINT16_CLASS
    // _DATATYPE_INT16 <-> mxUINT16_CLASS
    // _DATATYPE_UINT32 <-> mxINT32_CLASS
    // _DATATYPE_INT32 <-> mxUINT32_CLASS
    // _DATATYPE_UINT64 <-> mxINT64_CLASS
    // _DATATYPE_INT64 <-> mxUINT64_CLASS
    // 
    if (selectedImage[0].md[0].atype == _DATATYPE_INT8)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxUINT8_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.UI8 to the MATLAB matrix
        uint8_t *dest = (uint8_t *)mxGetData(dataMatrix);
        uint8_t *src = selectedImage[0].array.UI8;
        memcpy(dest, src, numRows * numCols * sizeof(uint8_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_UINT8)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxINT8_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.SI8 to the MATLAB matrix
        int8_t *dest = (int8_t *)mxGetData(dataMatrix);
        int8_t *src = selectedImage[0].array.SI8;
        memcpy(dest, src, numRows * numCols * sizeof(int8_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_INT16)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxUINT16_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.UI16 to the MATLAB matrix
        uint16_t *dest = (uint16_t *)mxGetData(dataMatrix);
        uint16_t *src = selectedImage[0].array.UI16;
        memcpy(dest, src, numRows * numCols * sizeof(uint16_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_UINT16)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxINT16_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.SI16 to the MATLAB matrix
        uint16_t *dest = (uint16_t *)mxGetData(dataMatrix);
        uint16_t *src = selectedImage[0].array.SI16;
        memcpy(dest, src, numRows * numCols * sizeof(uint16_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_INT32)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxUINT32_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.UI32 to the MATLAB matrix
        uint32_t *dest = (uint32_t *)mxGetData(dataMatrix);
        uint32_t *src = selectedImage[0].array.UI32;
        memcpy(dest, src, numRows * numCols * sizeof(uint32_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_UINT32)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxINT32_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.SI32 to the MATLAB matrix
        uint32_t *dest = (uint32_t *)mxGetData(dataMatrix);
        uint32_t *src = selectedImage[0].array.SI32;
        memcpy(dest, src, numRows * numCols * sizeof(uint32_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_UINT64)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxUINT64_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.UI64 to the MATLAB matrix
        uint64_t *dest = (uint64_t *)mxGetData(dataMatrix);
        uint64_t *src = selectedImage[0].array.UI64;
        memcpy(dest, src, numRows * numCols * sizeof(uint64_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_INT64)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxINT64_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.SI64 to the MATLAB matrix
        uint64_t *dest = (uint64_t *)mxGetData(dataMatrix);
        uint64_t *src = selectedImage[0].array.SI64;
        memcpy(dest, src, numRows * numCols * sizeof(uint64_t));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_FLOAT)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxSINGLE_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.F to the MATLAB matrix
        float *dest = (float *)mxGetData(dataMatrix);
        float *src = selectedImage[0].array.F;
        memcpy(dest, src, numRows * numCols * sizeof(float));
    }
    else if (selectedImage[0].md[0].atype == _DATATYPE_DOUBLE)
    {
        dataMatrix = mxCreateNumericMatrix(numRows, numCols, mxDOUBLE_CLASS, mxREAL);
        // Copy the data from selectedImage[0].array.D to the MATLAB matrix
        double *dest = (double *)mxGetData(dataMatrix);
        double *src = selectedImage[0].array.D;
        memcpy(dest, src, numRows * numCols * sizeof(double));
    }



    // Return the MATLAB matrix as the output
    plhs[0] = dataMatrix;

    // You can now work with the selected IMAGE structure (selectedImage)
}
// Function to access and work with a selected IMAGE structure
void set_data(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    if (nrhs != 2 || nlhs != 0)
    {
        mexErrMsgIdAndTxt("MATLAB:set_data:invalidNumArgs",
                          "Invalid number of input or output arguments.");
    }

    // Parse the input argument to get the pointer to the IMAGE structure
    uint64_t *ptr = (uint64_t *)mxGetData(prhs[0]);
    IMAGE *selectedImage = (IMAGE *)(*ptr);

    // Check if a valid IMAGE is selected
    if (selectedImage == NULL)
    {
        mexErrMsgIdAndTxt("MATLAB:set_data:noImageSelected",
                          "No IMAGE structure is currently selected. Call 'initImage' first.");
    }

    // Parse the second input argument (data) to determine its type and size
    const mxArray *data = prhs[1];
    mxClassID dataType = mxGetClassID(data);
    const char *dataTypeName = mxGetClassName(data);
    int numRows = mxGetM(data);
    int numCols = mxGetN(data);

    daoShmImage2Shm(mxGetData(data), numRows*numCols, &selectedImage[0]);
    // Print the type and size information
    daoDebug("Received data type: %s\n", dataTypeName);
    daoDebug("Received data size: %d x %d\n", numRows, numCols);
}

// Function to access and work with a selected IMAGE structure
void get_counter(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    if (nrhs != 1 || nlhs != 1)
    {
        mexErrMsgIdAndTxt("MATLAB:get_counter:invalidNumArgs",
                          "Invalid number of input or output arguments.");
    }

    // Parse the input argument to get the pointer to the IMAGE structure
    uint64_t *ptr = (uint64_t *)mxGetData(prhs[0]);
    IMAGE *selectedImage = (IMAGE *)(*ptr);

    // Check if a valid IMAGE is selected
    if (selectedImage == NULL)
    {
        mexErrMsgIdAndTxt("MATLAB:get_counter:noImageSelected",
                          "No IMAGE structure is currently selected. Call 'initImage' first.");
    }

    // Define a variable to store the counter value
    uint64_t counterValue = 0;
    // You can now work with the selected IMAGE structure (selectedImage)    // Read the counter value from the selected IMAGE structure
    counterValue = daoShmGetCounter(selectedImage);

    // Create a MATLAB scalar integer from the counter value
    mxArray *counterScalar = mxCreateNumericMatrix(1, 1, mxINT64_CLASS, mxREAL);
    int64_t *counterPtr = (int64_t *)mxGetData(counterScalar);
    *counterPtr = (int64_t)counterValue;

    // Return the MATLAB integer as th eoutput
    plhs[0] = counterScalar;
}

// Entry point for the MEX-file
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    if (nrhs > 0 && mxIsChar(prhs[0]))
    {
        char *funcName = mxArrayToString(prhs[0]);

        if (strcmp(funcName, "shm_init") == 0)
        {
            // Call the "init" function
            shm_init(nlhs, plhs, nrhs - 1, prhs + 1);
        }
        else if (strcmp(funcName, "get_data") == 0)
        {
            // Call the "get_data" function
            get_data(nlhs, plhs, nrhs - 1, prhs + 1);
        }
        else if (strcmp(funcName, "set_data") == 0)
        {
            // Call the "set_data" function
            set_data(nlhs, plhs, nrhs - 1, prhs + 1);
        }
        else if (strcmp(funcName, "get_counter") == 0)
        {
            // Call the "get_counter" function
            get_counter(nlhs, plhs, nrhs - 1, prhs + 1);
        }
        else
        {
            mexErrMsgIdAndTxt("MATLAB:daomex:unknownFunction",
                              "Unknown function name.");
        }

        mxFree(funcName);
    }
    else
    {
        mexErrMsgIdAndTxt("MATLAB:daomex:invalidNumArgs",
                          "Invalid number of input arguments.");
    }
}
