#ifndef ATMO_LIBC_MATH_H
#define ATMO_LIBC_MATH_H

/* Code in an Atmosphere process cannot use floating point. The declaration
 * lets headers that name fabs() in unused macros compile. */
double fabs(double x);

#endif
