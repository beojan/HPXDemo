#ifndef CPUMTRX_H_
#define CPUMTRX_H_
// This defines a CPUMtrx type

#include <cblas.h>
#include <mkl_vsl.h>
#include <cstdlib>
#include <cstring>

void setup() {
    // no-op
}

void teardown() {
    // no-op
}

template <int Rows> class CPUMtrx {
  private:
    float* devPtr = nullptr;

  public:
    static constexpr int Size = Rows * Rows;

    CPUMtrx() {
        devPtr = (float*)malloc(Size * sizeof(float));
        memset(devPtr, 0, Size * sizeof(float));
    }
    CPUMtrx(const CPUMtrx&) = delete;
    CPUMtrx(CPUMtrx&&) = default;
    CPUMtrx(long long mult) {
        static thread_local VSLStreamStatePtr stream = nullptr;
        if (!stream) {
            vslNewStream(&stream, VSL_BRNG_MT19937, 777);
        }
        float mult_f = mult;
        devPtr = (float*)malloc(Size * sizeof(float));
        vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, stream, Size, devPtr, 1e-7f, 1.f);
        cblas_sscal(Size, mult_f, devPtr, 1);
    }
    ~CPUMtrx() { free((void*)devPtr); }

    CPUMtrx operator*(const CPUMtrx& rhs) {
        CPUMtrx result{};
        const float alpha = 1;
        const float beta = 0;
        cblas_sgemm(CBLAS_LAYOUT::CblasColMajor, CBLAS_TRANSPOSE::CblasNoTrans,
                    CBLAS_TRANSPOSE::CblasNoTrans, Rows, Rows, Rows, alpha, this->devPtr, Rows,
                    rhs.devPtr, Rows, beta, result.devPtr, Rows);
        return result;
    }

    CPUMtrx operator+(const CPUMtrx& rhs) {
        CPUMtrx result{};
        const float alpha = 1;
        cblas_scopy(Size, this->devPtr, 1, result.devPtr, 1);
        cblas_saxpy(Size, alpha, rhs.devPtr, 1, result.devPtr, 1);
        return result;
    }

    float norm() {
        float result = 0.f;
        result = cblas_snrm2(Size, this->devPtr, 1);
        return result;
    }
};

#endif // CPUMTRX_H_
