/*
 * These functions are supposed to pre-check simple integer math operators
 * for overflows without triggering the 'overflow optimisations' that
 * GCC does. Obviously this is very expensive so I attempt to use this
 * as little as possible.
 *
 * This is a very difficult thing to do so I've copied the code from here:
 * https://www.securecoding.cert.org/confluence/display/seccode/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow
 *
 * Hopefully I haven't introduced any bugs.
 *
 * Note: for some reason the __builtin.*overflow functions do not implement
 * efficient (twos-complement) results when they overflow.
 */

/* See if we have any help from the compiler */
#ifndef __has_builtin
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers.
#endif

static inline
signed int ov_imul(signed int a, signed int b, int * ov) {

#if __has_builtin(__builtin_smul_overflow)
  int r = 0;
  *ov = *ov || __builtin_smul_overflow(a, b, &r);
  if (ov) return (int)((unsigned int)a * (unsigned int)b);
  return r;
#else
  if (a > 0) {  /* a is positive */
    if (b > 0) {  /* a and b are positive */
      if (a > (INT_MAX / b)) {
        /* Handle error */
	*ov = 1;
	return (int)((unsigned int)a * (unsigned int)b);
      }
    } else { /* a positive, b nonpositive */
      if (b < (INT_MIN / a)) {
        /* Handle error */
	*ov = 1;
	return (int)((unsigned int)a * (unsigned int)b);
      }
    } /* a positive, b nonpositive */
  } else { /* a is nonpositive */
    if (b > 0) { /* a is nonpositive, b is positive */
      if (a < (INT_MIN / b)) {
        /* Handle error */
	*ov = 1;
	return (int)((unsigned int)a * (unsigned int)b);
      }
    } else { /* a and b are nonpositive */
      if ( (a != 0) && (b < (INT_MAX / a))) {
        /* Handle error */
	*ov = 1;
	return (int)((unsigned int)a * (unsigned int)b);
      }
    } /* End if a and b are nonpositive */
  } /* End if a is nonpositive */

  return a * b;
#endif
}

static inline
signed int ov_iadd(signed int a, signed int b, int * ov) {

#if __has_builtin(__builtin_sadd_overflow)
  int r = 0;
  *ov = *ov || __builtin_sadd_overflow(a, b, &r);
  if (ov) return (int)((unsigned int)a + (unsigned int)b);
  return r;
#else

  if (((b > 0) && (a > (INT_MAX - b))) ||
      ((b < 0) && (a < (INT_MIN - b)))) {
    /* Handle error */
    *ov = 1;
    return (int)((unsigned int)a + (unsigned int)b);
  }
  return a + b;
#endif
}

static inline
signed int ov_isub(signed int a, signed int b, int * ov) {

#if __has_builtin(__builtin_ssub_overflow)
  int r = 0;
  *ov = *ov || __builtin_ssub_overflow(a, b, &r);
  if (ov) return (int)((unsigned int)a - (unsigned int)b);
  return r;
#else

  if ((b > 0 && a < INT_MIN + b) ||
      (b < 0 && a > INT_MAX + b)) {
    /* Handle error */
    *ov = 1;
    return (int)((unsigned int)a - (unsigned int)b);
  }
  return a - b;
#endif
}

static inline
signed int ov_idiv(signed int a, signed int b, int * ov) {

  if ((b == 0) || ((a == INT_MIN) && (b == -1))) {
    /* Handle error */
    *ov = 1;
    if (b == 0) return 0; else return a;
  }
  return a / b;
}

static inline
signed int ov_imod(signed int a, signed int b, int * ov) {

  if ((b == 0 ) || ((a == INT_MIN) && (b == -1))) {
    /* Handle error */
    *ov = 1;
    if (b == 0) return a; else return 0;
  }
  return a % b;
}

static inline
signed int ov_ineg(signed int a, int * ov) {

#if __has_builtin(__builtin_ssub_overflow)
  int r = 0;
  *ov = *ov || __builtin_ssub_overflow(0, a, &r);
  if (ov) return (int)((unsigned int)0 - (unsigned int)a);
  return r;
#else

  if (a == INT_MIN) {
      *ov = 1;
      return INT_MIN;
  }
  return -a;
#endif
}
