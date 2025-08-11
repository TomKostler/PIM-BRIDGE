#ifndef BINS_H
#define BINS_H

struct pim_vectors {
    uint64_t offset_a;
    uint64_t offset_b;
    uint64_t result_offset;
    uint32_t len;
};

struct pim_gemv {
    __u64 input_vector_user_addr;
    __u64 matrix_user_addr;
    __u64 result_vector_user_addr;
    __u32 input_vector_len;
    __u32 matrix_dim1;
    __u32 matrix_dim2;
};

void vadd_driver_code(void);
void vmul_driver_code(void);
void gemv_driver_code(void);

int vadd_from_userspace(uint16_t *vector_a_address, uint16_t *vector_b_address,
                        struct pim_vectors *vectors_descriptor);
int vmul_from_userspace(uint16_t *vector_a_address, uint16_t *vector_b_address,
                        struct pim_vectors *vectors_descriptor);
int gemv_from_userspace(__u64 result_addr, uint16_t *input_vector_data,
                        uint16_t *matrix_data, uint32_t len_input_vector,
                        uint32_t matrix_rows, uint32_t matrix_cols);

#endif