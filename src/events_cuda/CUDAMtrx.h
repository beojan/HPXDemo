#ifndef CUDAMTRX_H_
#define CUDAMTRX_H_
// This defines a CUDAMtrx type

#include <cublas_v2.h>
#include <cuda.h>
#include <curand.h>

#include <fmt/format.h>

extern cublasHandle_t cublas_hndl;
extern curandGenerator_t curand_gen;

#define CU_CHECK(X)                                                                                \
    {                                                                                              \
        auto custatus = (X);                                                                       \
        if (custatus != cudaSuccess) {                                                             \
            fmt::print("CUDA ERROR {} -- {} (in {})\n", cudaGetErrorName(custatus),                \
                       cudaGetErrorString(custatus), (#X));                                        \
        }                                                                                          \
        else {                                                                                     \
            fmt::print("CUDA SUCCESS\n");                                                          \
        }                                                                                          \
        cudaGetLastError();                                                                        \
    } // comment to absorb ;

#define CUB_CHECK(X)                                                                               \
    {                                                                                              \
        auto custatus = (X);                                                                       \
        if (custatus != CUBLAS_STATUS_SUCCESS) {                                                   \
            fmt::print("CUBLAS ERROR {} -- {} (in {})\n", cublasGetStatusName(custatus),           \
                       cublasGetStatusString(custatus), (#X));                                     \
        }                                                                                          \
    } // comment to absorb ;
const char* curandGetErrorString(curandStatus_t error) {
    switch (error) {
    case CURAND_STATUS_SUCCESS:
        return "CURAND_STATUS_SUCCESS";

    case CURAND_STATUS_VERSION_MISMATCH:
        return "CURAND_STATUS_VERSION_MISMATCH";

    case CURAND_STATUS_NOT_INITIALIZED:
        return "CURAND_STATUS_NOT_INITIALIZED";

    case CURAND_STATUS_ALLOCATION_FAILED:
        return "CURAND_STATUS_ALLOCATION_FAILED";

    case CURAND_STATUS_TYPE_ERROR:
        return "CURAND_STATUS_TYPE_ERROR";

    case CURAND_STATUS_OUT_OF_RANGE:
        return "CURAND_STATUS_OUT_OF_RANGE";

    case CURAND_STATUS_LENGTH_NOT_MULTIPLE:
        return "CURAND_STATUS_LENGTH_NOT_MULTIPLE";

    case CURAND_STATUS_DOUBLE_PRECISION_REQUIRED:
        return "CURAND_STATUS_DOUBLE_PRECISION_REQUIRED";

    case CURAND_STATUS_LAUNCH_FAILURE:
        return "CURAND_STATUS_LAUNCH_FAILURE";

    case CURAND_STATUS_PREEXISTING_FAILURE:
        return "CURAND_STATUS_PREEXISTING_FAILURE";

    case CURAND_STATUS_INITIALIZATION_FAILED:
        return "CURAND_STATUS_INITIALIZATION_FAILED";

    case CURAND_STATUS_ARCH_MISMATCH:
        return "CURAND_STATUS_ARCH_MISMATCH";

    case CURAND_STATUS_INTERNAL_ERROR:
        return "CURAND_STATUS_INTERNAL_ERROR";
    }

    return "<unknown>";
}

#define CUR_CHECK(X)                                                                               \
    {                                                                                              \
        auto custatus = (X);                                                                       \
        if (custatus != CURAND_STATUS_SUCCESS) {                                                   \
            fmt::print("CURAND ERROR {} (in {})\n", curandGetErrorString(custatus), (#X));         \
        }                                                                                          \
    } // comment to absorb ;

void setup() {
    cudaGetLastError();
    CUB_CHECK(cublasCreate_v2(&cublas_hndl));
    CUR_CHECK(curandCreateGenerator(&curand_gen, CURAND_RNG_PSEUDO_DEFAULT));
    CU_CHECK(cudaDeviceSynchronize());
}

void teardown() {
    CU_CHECK(cudaDeviceSynchronize());
    CUR_CHECK(curandDestroyGenerator(curand_gen));
    CUB_CHECK(cublasDestroy_v2(cublas_hndl));
}

template <int Rows> class CUDAMtrx {
  private:
    float* devPtr = nullptr;

  public:
    static constexpr int Size = Rows * Rows;

    CUDAMtrx() {
        CU_CHECK(cudaMalloc(&devPtr, Size * sizeof(float)));
        CU_CHECK(cudaMemset(devPtr, 0, Size * sizeof(float)));
        CU_CHECK(cudaDeviceSynchronize());
    }
    CUDAMtrx(const CUDAMtrx&) = delete;
    CUDAMtrx(CUDAMtrx&&) = default;
    CUDAMtrx(long long mult) {
        float mult_f = mult;
        CU_CHECK(cudaMalloc(&devPtr, Size * sizeof(float)));
        CUR_CHECK(curandGenerateUniform(curand_gen, devPtr, Size));
        CUB_CHECK(cublasSscal_v2(cublas_hndl, Size, &mult_f, devPtr, 1));
        CU_CHECK(cudaDeviceSynchronize());
    }
    ~CUDAMtrx() { CU_CHECK(cudaFree((void*)devPtr)); }

    CUDAMtrx operator*(const CUDAMtrx& rhs) {
        CUDAMtrx result{};
        const float alpha = 1;
        const float beta = 0;
        CUB_CHECK(cublasSgemm_v2(cublas_hndl, CUBLAS_OP_N, CUBLAS_OP_N, Rows, Rows, Rows, &alpha,
                                 this->devPtr, Rows, rhs.devPtr, Rows, &beta, result.devPtr, Rows));
        return result;
    }

    CUDAMtrx operator+(const CUDAMtrx& rhs) {
        CUDAMtrx result{};
        const float alpha = 1;
        CUB_CHECK(cublasScopy(cublas_hndl, Size, this->devPtr, 1, result.devPtr, 1));
        CUB_CHECK(cublasSaxpy_v2(cublas_hndl, Size, &alpha, rhs.devPtr, 1, result.devPtr, 1));
        return result;
    }

    float norm() {
        float result = 0.f;
        CUB_CHECK(cublasSnrm2_v2(cublas_hndl, Size, this->devPtr, 1, &result));
        return result;
    }
};

#endif // CUDAMTRX_H_
