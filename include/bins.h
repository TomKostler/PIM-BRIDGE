#ifndef BINS_H
#define BINS_H

void vadd_driver_code(void);
void vmul_driver_code(void);
void gemv_driver_code(void);

int vadd_from_userspace(__u64 result_addr, uint16_t *vector_arr_a,
                        uint16_t *vector_arr_b, int len);
int vmul_from_userspace(__u64 result_addr, uint16_t *vector_arr_a,
                        uint16_t *vector_arr_b, int len);
int gemv_from_userspace(__u64 result_addr, uint16_t *input_vector_data,
                        uint16_t *matrix_data, uint32_t len_input_vector,
                        uint32_t matrix_rows, uint32_t matrix_cols);

#endif