#ifndef BINS_H
#define BINS_H

void vadd_driver_code(void);
void vmul_driver_code(void);
void gemv_driver_code(void);

int vadd_from_userspace(__u64 result_addr, uint16_t *vector_arr_a,
                         uint16_t *vector_arr_b, int len);
int vmul_from_userspace(__u64 result_addr, uint16_t *vector_arr_a,
                         uint16_t *vector_arr_b, int len);
int gemv_from_userspace(__u64 result_addr, uint16_t *kernel_input_vector,
                         uint16_t *kernel_matrix, uint32_t len_input_vector,
                         uint32_t matrix_dim1, uint32_t matrix_dim2);

#endif