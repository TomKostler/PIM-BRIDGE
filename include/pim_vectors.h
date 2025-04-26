#ifndef PIM_VECTORS_H
#define PIM_VECTORS_H


#define ROWS 256


/*
    Initializes the vectors needed for vadd
*/
int init_vector(uint16_t* arr, size_t length);

/*
    Initializes a vector filled with zeros so that the result can be written in it
*/
int init_vector_result(size_t length);



void half_to_string(uint16_t half, char *str);


#endif