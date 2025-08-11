#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_BLACK "\x1b[30m"
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_WHITE "\x1b[37m"

#define STYLE_BOLD "\x1b[1m"

struct pim_vectors {
    uint64_t offset_a;
    uint64_t offset_b;
    uint64_t result_offset;
    uint32_t len;
};

struct pim_gemv {
    uint64_t input_vector_user_addr;
    uint64_t matrix_user_addr;
    uint64_t result_vector_user_addr;
    uint32_t input_vector_len;
    uint32_t matrix_dim1;
    uint32_t matrix_dim2;
};

#define MAJOR_NUM 100
#define DEVICE_PATH "/dev/pim_device"
#define IOCTL_VADD _IOWR(MAJOR_NUM, 2, struct pim_vectors)
#define IOCTL_VMUL _IOWR(MAJOR_NUM, 3, struct pim_vectors)
#define IOCTL_GEMV _IOWR(MAJOR_NUM, 4, struct pim_gemv)

typedef union {
    float f;
    uint32_t u;
} FloatUnion;

uint16_t float_to_f16(float f) {
    FloatUnion fu;
    fu.f = f;

    // Extract sign, exponent and mantissa from 32-bit float
    uint16_t sign = (fu.u >> 16) & 0x8000;
    int32_t exponent =
        ((fu.u >> 23) & 0xFF) - 127; // Exponent with a bias of 127
    uint32_t mantissa = fu.u & 0x7FFFFF;

    // Handle Infinity and NaN cases
    if (exponent == 128) {
        return sign | (0x1F << 10) | (mantissa ? 0x200 : 0);
    }

    // Handle underflow to zero
    if (exponent < -24) {
        return sign;
    }

    // Handle numbers that become denormalized in f16
    if (exponent < -14) {
        mantissa = (mantissa | 0x800000) >> (14 - abs(exponent));
        return sign | (mantissa >> 13);
    }

    // Handle overflow to infinity
    if (exponent > 15) {
        return sign | (0x1F << 10);
    }

    // Process normal numbers
    uint16_t new_exponent = (exponent + 15) & 0x1F;
    uint16_t new_mantissa = mantissa >> 13;

    return sign | (new_exponent << 10) | new_mantissa;
}

float f16_to_float(uint16_t f16_val) {
    FloatUnion fu;

    // Extract sign, exponent, and mantissa from the 16-bit float
    uint32_t sign = (f16_val >> 15) & 0x01;
    int32_t exponent = (f16_val >> 10) & 0x1F;
    uint32_t mantissa = f16_val & 0x03FF;

    // Handle special cases
    if (exponent == 0x1F) { // Infinity or NaN
        // Set all exponent bits for f32
        fu.u = (sign << 31) | 0x7F800000 | (mantissa << 13);
        return fu.f;
    }
    if (exponent == 0) { // Zero or denormalized number
        if (mantissa == 0) {
            fu.u = (sign << 31);
            return fu.f; // +/- 0.0f
        }
        // It's a denormalized number => normalize it
        while (!(mantissa & 0x0400)) {
            mantissa <<= 1;
            exponent--;
        }
        mantissa &= 0x03FF;
        exponent++;
    }

    // Process normalized numbers
    exponent = (exponent - 15) + 127;
    mantissa <<= 13;

    // Combine parts into the 32-bit float format
    fu.u = (sign << 31) | (exponent << 23) | mantissa;

    return fu.f;
}

void print_vector_operation(const char *title, const uint16_t *vec_a,
                            const uint16_t *vec_b, const uint16_t *vec_res,
                            uint32_t len) {
    printf("\n" STYLE_BOLD COLOR_YELLOW "--- %s ---" COLOR_RESET "\n", title);
    printf(COLOR_BLUE "========================================================"
                      "\n" COLOR_RESET);
    printf(COLOR_CYAN "%-6s | %-12s | %-12s | %-12s\n" COLOR_RESET, "Index",
           "Vector a", "Vector b", "result");
    printf(
        COLOR_BLUE
        "-------+--------------+--------------+--------------\n" COLOR_RESET);

    uint32_t print_len = (len > 2048) ? 10 : len;
    for (int i = 0; i < print_len; i++) {
        printf("%-6d | %-12.4f | %-12.4f | " COLOR_GREEN "%-12.4f" COLOR_RESET
               "\n",
               i, f16_to_float(vec_a[i]), f16_to_float(vec_b[i]),
               f16_to_float(vec_res[i]));
    }
    if (len > 2048) {
        printf(COLOR_BLUE "... [first 10 Elements printed]\n" COLOR_RESET);
    }
    printf(COLOR_BLUE "========================================================"
                      "\n\n\n" COLOR_RESET);
}

void print_gemv_operation(const char *title, const uint16_t *input_vec,
                          uint32_t input_vec_len, const uint16_t *matrix,
                          uint32_t matrix_rows, uint32_t matrix_cols,
                          const uint16_t *result_vec, uint32_t result_vec_len) {

    const int preview_size = 6;

    printf("\n" STYLE_BOLD COLOR_YELLOW "--- %s ---" COLOR_RESET "\n", title);
    printf(COLOR_BLUE "========================================================"
                      "\n" COLOR_RESET);

    printf(COLOR_CYAN "Input Vector (length: %u):" COLOR_RESET "\n[",
           input_vec_len);
    for (int i = 0; i < input_vec_len; ++i) {
        if (i < preview_size || i >= input_vec_len - preview_size) {
            printf(COLOR_YELLOW " %.2f" COLOR_RESET,
                   f16_to_float(input_vec[i]));
        } else if (i == preview_size) {
            printf(" ...");
        }
    }
    printf(" ]\n\n");

    printf(COLOR_CYAN "Matrix (Dimension: %u x %u):" COLOR_RESET "\n",
           matrix_rows, matrix_cols);
    for (int r = 0; r < matrix_rows; ++r) {
        if (r < preview_size || r >= matrix_rows - preview_size) {
            printf(" [");
            for (int c = 0; c < matrix_cols; ++c) {
                if (c < preview_size || c >= matrix_cols - preview_size) {
                    printf(COLOR_MAGENTA "%6.1f" COLOR_RESET,
                           f16_to_float(matrix[(size_t)r * matrix_cols + c]));
                } else if (c == preview_size) {
                    printf("  ...  ");
                }
            }
            printf(" ]\n");
        } else if (r == preview_size) {
            printf("   (...)\n");
        }
    }
    printf("\n");

    printf(COLOR_CYAN "Result Vector (length: %u):" COLOR_RESET "\n",
           result_vec_len);
    for (int i = 0; i < result_vec_len; ++i) {
        if (i > 0 && i % 8 == 0) {
            printf("\n");
        }
        printf(STYLE_BOLD COLOR_GREEN " %8.2f" COLOR_RESET,
               f16_to_float(result_vec[i]));
    }
    printf("\n");
    printf(COLOR_BLUE "========================================================"
                      "\n\n\n" COLOR_RESET);
}

void test_gemv_identity_matrix(int fd, uint32_t dim) {
    printf(STYLE_BOLD "\n--- GEMV Test: Identity Matrix ---\n" COLOR_RESET);
    if (dim % 16 != 0) {
        printf("Dimension must be multiple of 16.\n");
        return;
    }

    uint16_t *vec = malloc(dim * sizeof(uint16_t));
    uint16_t *mat = calloc(dim * dim, sizeof(uint16_t));
    uint16_t *res = malloc(dim * sizeof(uint16_t));
    if (!vec || !mat || !res) {
        perror("malloc/calloc failed");
        free(vec);
        free(mat);
        free(res);
        return;
    }

    for (uint32_t i = 0; i < dim; i++) {
        // vec[i] = float_to_f16((float)i);
        vec[i] = float_to_f16(5.0f);
    }

    for (uint32_t i = 0; i < dim; i++) {
        mat[i * dim + i] = float_to_f16(1.0f);
    }

    struct pim_gemv desc;
    desc.input_vector_user_addr = (uint64_t)vec;
    desc.matrix_user_addr = (uint64_t)mat;
    desc.result_vector_user_addr = (uint64_t)res;
    desc.input_vector_len = dim;
    desc.matrix_dim1 = dim;
    desc.matrix_dim2 = dim;

    if (ioctl(fd, IOCTL_GEMV, &desc) < 0) {
        perror("ioctl(IOCTL_GEMV) failed");
    } else {
        print_gemv_operation("GEMV (Identity test)", vec, dim, mat, dim, dim,
                             res, dim);
    }

    free(vec);
    free(mat);
    free(res);
}

void gemv_arbitrary_dims(int fd, int rows, int cols) {
    struct pim_gemv pim_gemv_desc;
    uint16_t *result_vector_gemv = NULL;
    uint16_t *matrix_data = NULL;
    uint16_t *input_vector_data = NULL;

    // --- Prepare GEMV data ---
    result_vector_gemv = malloc(rows * sizeof(uint16_t));
    input_vector_data = malloc(cols * sizeof(uint16_t));
    matrix_data = calloc(rows * cols, sizeof(uint16_t));

    if (!result_vector_gemv || !input_vector_data || !matrix_data) {
        perror("malloc/calloc for GEMV data failed");
        free(result_vector_gemv);
        free(input_vector_data);
        free(matrix_data);
        return;
    }

    for (int i = 0; i < cols; ++i) {
        input_vector_data[i] = float_to_f16(1);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r >= c) {
                matrix_data[(size_t)r * cols + c] = float_to_f16(1);
            }
        }
    }

    pim_gemv_desc.input_vector_user_addr = (uint64_t)input_vector_data;
    pim_gemv_desc.matrix_user_addr = (uint64_t)matrix_data;
    pim_gemv_desc.input_vector_len = cols;
    pim_gemv_desc.result_vector_user_addr = (uint64_t)result_vector_gemv;
    pim_gemv_desc.matrix_dim1 = rows;
    pim_gemv_desc.matrix_dim2 = cols;

    printf("Calling GEMV for %d x %d matrix...\n", rows, cols);
    if (ioctl(fd, IOCTL_GEMV, &pim_gemv_desc) < 0) {
        perror("ioctl(IOCTL_GEMV) failed");
    } else {
        print_gemv_operation("GEMV", input_vector_data, cols, matrix_data, rows,
                             cols, result_vector_gemv, rows);
    }

    free(result_vector_gemv);
    free(input_vector_data);
    free(matrix_data);
}

void vector_add_userspace(const float *a, const float *b, float *result,
                          int len) {
    for (int i = 0; i < len; i++) {
        result[i] = a[i] + b[i];
    }
}

void vadd_userspace_evaluation(uint32_t vector_len) {
    system("gem5-bridge --addr=0x10010000 resetstats");
    float *vector_arr_a = NULL;
    float *vector_arr_b = NULL;
    float *result_vector = NULL;

    vector_arr_a = malloc(vector_len * sizeof(float));
    vector_arr_b = malloc(vector_len * sizeof(float));
    result_vector = malloc(vector_len * sizeof(float));

    if (!vector_arr_a || !vector_arr_b || !result_vector) {
        perror("malloc for vectors failed");
        free(vector_arr_a);
        free(vector_arr_b);
        free(result_vector);
        return;
    }

    float pattern_a[] = {0.0f, 1.0f, 2.0f};
    for (int i = 0; i < vector_len; i++) {
        vector_arr_a[i] = pattern_a[i % 3];
    }

    float pattern_b[] = {1.0f, 2.0f, 0.0f};
    for (int i = 0; i < vector_len; i++) {
        vector_arr_b[i] = pattern_b[i % 3];
    }

    vector_add_userspace(vector_arr_a, vector_arr_b, result_vector, vector_len);
    system("gem5-bridge --addr=0x10010000 dumpstats");

    printf("Index | Vector A | Vector B | Result\n");
    printf("------|----------|----------|----------\n");
    for (int i = 0; i < 6; i++) {
        printf("%-5d | %-8.1f | %-8.1f | %-8.1f\n", i, (float)vector_arr_a[i],
               (float)vector_arr_b[i], (float)result_vector[i]);
    }

    free(vector_arr_a);
    free(vector_arr_b);
    free(result_vector);
}

void vector_vmul_userspace(const float *a, const float *b, float *result,
                           int len) {
    for (int i = 0; i < len; i++) {
        result[i] = a[i] * b[i];
    }
}

void vmul_userspace_evaluation(uint32_t vector_len) {
    system("gem5-bridge --addr=0x10010000 resetstats");
    float *vector_arr_a = NULL;
    float *vector_arr_b = NULL;
    float *result_vector = NULL;

    vector_arr_a = malloc(vector_len * sizeof(float));
    vector_arr_b = malloc(vector_len * sizeof(float));
    result_vector = malloc(vector_len * sizeof(float));

    if (!vector_arr_a || !vector_arr_b || !result_vector) {
        perror("malloc for vectors failed");
        free(vector_arr_a);
        free(vector_arr_b);
        free(result_vector);
        return;
    }

    float pattern_a[] = {0.0f, 1.0f, 2.0f};
    for (int i = 0; i < vector_len; i++) {
        vector_arr_a[i] = pattern_a[i % 3];
    }

    float pattern_b[] = {1.0f, 2.0f, 0.0f};
    for (int i = 0; i < vector_len; i++) {
        vector_arr_b[i] = pattern_b[i % 3];
    }

    vector_vmul_userspace(vector_arr_a, vector_arr_b, result_vector,
                          vector_len);
    system("gem5-bridge --addr=0x10010000 dumpstats");

    printf("Index | Vector A | Vector B | Result\n");
    printf("------|----------|----------|----------\n");
    for (int i = 0; i < 6; i++) {
        printf("%-5d | %-8.1f | %-8.1f | %-8.1f\n", i, (float)vector_arr_a[i],
               (float)vector_arr_b[i], (float)result_vector[i]);
    }

    free(vector_arr_a);
    free(vector_arr_b);
    free(result_vector);
}

void matrix_vector_mul(const float *matrix, const float *vector, float *result,
                       int rows, int cols) {
    for (int r = 0; r < rows; ++r) {
        float sum = 0.0f;
        for (int c = 0; c < cols; ++c) {
            sum += matrix[(size_t)r * cols + c] * vector[c];
        }
        result[r] = sum;
    }
}

void gemv_userspace_evaluation(int rows, int cols) {
    system("gem5-bridge --addr=0x10010000 resetstats");

    float *result_vector_gemv = NULL;
    float *matrix_data = NULL;
    float *input_vector_data = NULL;

    result_vector_gemv = malloc((size_t)rows * sizeof(float));
    matrix_data = calloc((size_t)rows * cols, sizeof(float));
    input_vector_data = malloc((size_t)cols * sizeof(float));

    if (!matrix_data || !result_vector_gemv || !input_vector_data) {
        perror("malloc/calloc for GEMV data failed");
        free(matrix_data);
        free(result_vector_gemv);
        free(input_vector_data);
        return;
    }

    for (int i = 0; i < cols; ++i) {
        input_vector_data[i] = 1.0f;
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r >= c) {
                matrix_data[(size_t)r * cols + c] = 1.0f;
            }
        }
    }

    matrix_vector_mul(matrix_data, input_vector_data, result_vector_gemv, rows,
                      cols);
    system("gem5-bridge --addr=0x10010000 dumpstats");

    printf("Result gemv userspace):\n");
    for (int i = 0; i < rows; ++i) {
        printf("Result[%-2d] = %.1f\n", i, result_vector_gemv[i]);
    }

    free(matrix_data);
    free(result_vector_gemv);
    free(input_vector_data);
}

void vadd_with_pim_evaluation(int fd, uint32_t vector_len) {
    struct pim_vectors pim_vectors_desc;

    size_t vector_size_bytes = vector_len * sizeof(uint16_t);
    size_t map_size = vector_size_bytes * 3;

    uint16_t *local_a = malloc(vector_size_bytes);
    uint16_t *local_b = malloc(vector_size_bytes);

    // Prepare fast all-in one memcpy through this indirection
    uint16_t pattern_a[] = {float_to_f16(0), float_to_f16(1), float_to_f16(2)};
    for (int i = 0; i < vector_len; i++) {
        local_a[i] = pattern_a[i % 3];
    }

    uint16_t pattern_b[] = {float_to_f16(1), float_to_f16(2), float_to_f16(0)};
    for (int i = 0; i < vector_len; i++) {
        local_b[i] = pattern_b[i % 3];
    }

    uint16_t *vector_arr_a =
        mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    uint16_t *vector_arr_b =
        (uint16_t *)((char *)vector_arr_a + vector_size_bytes);
    uint16_t *result_ptr =
        (uint16_t *)((char *)vector_arr_a + 2 * vector_size_bytes);

    system("gem5-bridge --addr=0x10010000 resetstats");

    // Faster than naive copying with the for loop directly to the mapped region
    memcpy(vector_arr_a, local_a, vector_size_bytes);
    memcpy(vector_arr_b, local_b, vector_size_bytes);

    printf("Calling IOCTL_VADD...\n");
    pim_vectors_desc.offset_a = 0;
    pim_vectors_desc.offset_b = vector_size_bytes;
    pim_vectors_desc.result_offset = 2 * vector_size_bytes;
    pim_vectors_desc.len = vector_len;

    printf("Calling VADD for Evaluation (Zero-Copy), vector length: %d\n",
           vector_len);
    if (ioctl(fd, IOCTL_VADD, &pim_vectors_desc) < 0) {
        perror("ioctl(IOCTL_VADD) failed");
    } else {
        system("gem5-bridge --addr=0x10010000 dumpstats");

        print_vector_operation("Vector Addition (VADD)", vector_arr_a,
                               vector_arr_b, result_ptr, vector_len);
    }

    // --- Cleanup ---
    munmap(vector_arr_a, map_size);
    free(local_a);
    free(local_b);
}

void vmul_with_pim_evaluation(int fd, uint32_t vector_len) {
    struct pim_vectors pim_vectors_desc;

    size_t vector_size_bytes = vector_len * sizeof(uint16_t);
    size_t map_size = vector_size_bytes * 3;

    uint16_t *local_a = malloc(vector_size_bytes);
    uint16_t *local_b = malloc(vector_size_bytes);

    // Prepare fast all-in one memcpy through this indirection
    uint16_t pattern_a[] = {float_to_f16(0), float_to_f16(1), float_to_f16(2)};
    for (int i = 0; i < vector_len; i++) {
        local_a[i] = pattern_a[i % 3];
    }

    uint16_t pattern_b[] = {float_to_f16(1), float_to_f16(2), float_to_f16(0)};
    for (int i = 0; i < vector_len; i++) {
        local_b[i] = pattern_b[i % 3];
    }

    uint16_t *vector_arr_a =
        mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    uint16_t *vector_arr_b =
        (uint16_t *)((char *)vector_arr_a + vector_size_bytes);
    uint16_t *result_ptr =
        (uint16_t *)((char *)vector_arr_a + 2 * vector_size_bytes);

    system("gem5-bridge --addr=0x10010000 resetstats");

    // Faster than naive copying with the for loop directly to the mapped region
    memcpy(vector_arr_a, local_a, vector_size_bytes);
    memcpy(vector_arr_b, local_b, vector_size_bytes);

    printf("Calling IOCTL_VMUL...\n");
    pim_vectors_desc.offset_a = 0;
    pim_vectors_desc.offset_b = vector_size_bytes;
    pim_vectors_desc.result_offset = 2 * vector_size_bytes;
    pim_vectors_desc.len = vector_len;

    printf("Calling VMUL for Evaluation (Zero-Copy), vector length: %d\n",
           vector_len);
    if (ioctl(fd, IOCTL_VMUL, &pim_vectors_desc) < 0) {
        perror("ioctl(IOCTL_VMUL) failed");
    } else {
        system("gem5-bridge --addr=0x10010000 dumpstats");
        print_vector_operation("Vector Multiplication (VMUL)", vector_arr_a,
                               vector_arr_b, result_ptr, vector_len);
    }

    // --- Cleanup ---
    munmap(vector_arr_a, map_size);
    free(local_a);
    free(local_b);
}

void gemv_with_pim_evaluation(int fd, int rows, int cols) {
    system("gem5-bridge --addr=0x10010000 resetstats");

    struct pim_gemv pim_gemv_desc;
    uint16_t *result_vector_gemv = NULL;
    uint16_t *matrix_data = NULL;
    uint16_t *input_vector_data = NULL;

    // --- Prepare GEMV data ---
    result_vector_gemv = malloc(rows * sizeof(uint16_t));
    input_vector_data = malloc(128 * sizeof(uint16_t));
    matrix_data = calloc(rows * 128, sizeof(uint16_t));

    if (!result_vector_gemv || !input_vector_data || !matrix_data) {
        perror("malloc/calloc for GEMV data failed");
        free(result_vector_gemv);
        free(input_vector_data);
        free(matrix_data);
        return;
    }

    for (int i = 0; i < 128; ++i) {
        input_vector_data[i] = float_to_f16(1);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < 128; ++c) { // Since we are in Evaluation Mode
            if (r >= c) {
                matrix_data[(size_t)r * 128 + c] = float_to_f16(1);
            }
        }
    }

    pim_gemv_desc.input_vector_user_addr = (uint64_t)input_vector_data;
    pim_gemv_desc.matrix_user_addr = (uint64_t)matrix_data;
    pim_gemv_desc.input_vector_len = 128;
    pim_gemv_desc.result_vector_user_addr = (uint64_t)result_vector_gemv;
    pim_gemv_desc.matrix_dim1 = rows;
    pim_gemv_desc.matrix_dim2 = cols;

    printf("Calling GEMV for %d x %d matrix...\n", rows, cols);
    if (ioctl(fd, IOCTL_GEMV, &pim_gemv_desc) < 0) {
        perror("ioctl(IOCTL_GEMV) failed");
    } else {
        system("gem5-bridge --addr=0x10010000 dumpstats");
        print_gemv_operation("GEMV", input_vector_data, 128, matrix_data, rows,
                             128, result_vector_gemv, rows);
    }

    free(result_vector_gemv);
    free(input_vector_data);
    free(matrix_data);
}

int main() {
    int fd = -1;

    // --- Open the Device ---
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file.");
        goto cleanup;
    }

    vadd_with_pim_evaluation(fd, 1 << 18);
    vadd_with_pim_evaluation(fd, 1 << 19);
    vadd_with_pim_evaluation(fd, 1 << 20);
    vadd_with_pim_evaluation(fd, 1 << 21);

    // vadd_userspace_evaluation(1 << 18);
    // vadd_userspace_evaluation(1 << 19);
    // vadd_userspace_evaluation(1 << 20);
    // vadd_userspace_evaluation(1 << 21);

    vmul_with_pim_evaluation(fd, 1 << 18);
    vmul_with_pim_evaluation(fd, 1 << 19);
    vmul_with_pim_evaluation(fd, 1 << 20);
    vmul_with_pim_evaluation(fd, 1 << 21);

    // vmul_userspace_evaluation(1 << 18);
    // vmul_userspace_evaluation(1 << 19);
    // vmul_userspace_evaluation(1 << 20);
    // vmul_userspace_evaluation(1 << 21);

    gemv_with_pim_evaluation(fd, 1024, 4096);
    gemv_with_pim_evaluation(fd, 2048, 4096);
    gemv_with_pim_evaluation(fd, 4096, 8192);
    gemv_with_pim_evaluation(fd, 8192, 8192);

    // gemv_userspace_evaluation(1024, 4096);
    // gemv_userspace_evaluation(2048, 4096);
    // gemv_userspace_evaluation(4096, 8192);
    // gemv_userspace_evaluation(8192, 8192);

    // This can be used for normal operation, when no evaluation has to be made
    // But be careful, the Evaluation Mode has to be unset in gemv.c in the
    // module, otherwise GEMV not functioning correctly

    // gemv_arbitrary_dims(fd,1024, 4096);
    // test_vadd_negative(fd, vector_len);
    // test_vmul_special_values(fd, vector_len);
    // test_gemv_identity_matrix(fd, 128);

cleanup:
    printf("Cleaning up and exiting userspace programm.\n");
    if (fd >= 0) {
        close(fd);
    }
    return 1;
}
