#ifndef CUDAMTRX_H_
#define CUDAMTRX_H_
// This defines a CUDAMtrx type

#include <cublas_v2.h>
#include <cuda.h>
#include <curand.h>
extern cublasHandle_t cublas_hndl;
extern curandGenerator_t curand_gen;

void setup() {
    cublasCreate_v2(&cublas_hndl);
    curandCreateGenerator(&curand_gen, CURAND_RNG_PSEUDO_DEFAULT);
    cudaDeviceSynchronize();
}

void teardown() {
    cudaDeviceSynchronize();
    curandDestroyGenerator(curand_gen);
    cublasDestroy_v2(cublas_hndl);
}

template <int Rows> class CUDAMtrx {
  private:
    float* devPtr = nullptr;

  public:
    static constexpr int Size = Rows * Rows;

    CUDAMtrx() {
        cudaMalloc(&devPtr, Size);
        cudaMemset(devPtr, 0, Size);
        cudaDeviceSynchronize();
    }
    CUDAMtrx(const CUDAMtrx&) = delete;
    CUDAMtrx(CUDAMtrx&&) = default;
    CUDAMtrx(long long mult) {
        float mult_f = mult;
        cudaMalloc(&devPtr, Size);
        curandGenerateUniform(curand_gen, devPtr, Size);
        cublasSscal_v2(cublas_hndl, Size, &mult_f, devPtr, 1);
        cudaDeviceSynchronize();
    }
    ~CUDAMtrx() { cudaFree((void*)devPtr); }

    CUDAMtrx operator*(const CUDAMtrx& rhs) {
        CUDAMtrx result{};
        const float alpha = 1;
        const float beta = 0;
        cublasSgemm_v2(cublas_hndl, CUBLAS_OP_N, CUBLAS_OP_N, Rows, Rows, Rows, &alpha,
                       this->devPtr, Rows, rhs.devPtr, Rows, &beta, result.devPtr, Rows);
        return result;
    }

    CUDAMtrx operator+(const CUDAMtrx& rhs) {
        CUDAMtrx result{};
        const float alpha = 1;
        cublasScopy(cublas_hndl, Size, this->devPtr, 1, result.devPtr, 1);
        cublasSaxpy_v2(cublas_hndl, Size, &alpha, rhs.devPtr, 1, result.devPtr, 1);
        return result;
    }

    float norm() {
        float result = 0.f;
        cublasSnrm2_v2(cublas_hndl, Size, this->devPtr, 1, &result);
        return result;
    }
};

#endif // CUDAMTRX_H_
