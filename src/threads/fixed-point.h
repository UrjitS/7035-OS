#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

typedef int fixed_point;

#define FRACTION_BITS 15

#define convert_int_to_fixed_point(int_value) ((fixed_point)(int_value << FRACTION_BITS))

#define convert_fixed_point_to_int_round_towards_zero(fixed_point_value) (fixed_point_value >> FRACTION_BITS)

#define convert_fixed_point_to_int_round_to_nearest(fixed_point_value) (fixed_point_value >= 0 ? ((fixed_point_value + (1 << (FRACTION_BITS - 1))) >> FRACTION_BITS) : ((fixed_point_value - (1 << (FRACTION_BITS - 1))) >> FRACTION_BITS))

/* Add two fixed-point numbers */
#define add_fixed_point_numbers(fixed_point_number1, fixed_point_number2) (fixed_point_number1 + fixed_point_number2)
/* Add integer n to fixed-point number x */
#define add_int_to_fixed_point_number(fixed_point_number, int_value) (fixed_point_number + (int_value << FRACTION_BITS))

/* Subtract two fixed-point numbers */
#define subtract_fixed_point_numbers(fixed_point_number1, fixed_point_number2) (fixed_point_number1 - fixed_point_number2)
/* Subtract integer n from fixed-point number x */
#define subtract_int_from_fixed_point_number(fixed_point_number, int_value) (fixed_point_number - (int_value << FRACTION_BITS))

/* Multiply two fixed-point numbers */
#define multiply_fixed_point_numbers(fixed_point_number1, fixed_point_number2) (fixed_point_number1 * fixed_point_number2)
/* Multiply fixed-point number x by integer n */
#define multiply_fixed_point_numbers_by_int(fixed_point_number, int_value) ((fixed_point)(((int64_t) fixed_point_number) * int_value >> FRACTION_BITS))

/* Divide two fixed-point numbers */
#define divide_fixed_point_numbers(fixed_point_number1, fixed_point_number2) (fixed_point_number1 / fixed_point_number2)
/* Divide fixed-point number x by integer n */
#define divide_integer_by_fixed_point(int_value, fixed_point_number) ((fixed_point)((((int64_t) int_value) << FRACTION_BITS) / fixed_point_number))

#endif