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
 */

/* See if we have any help from the compiler */
#ifndef __has_builtin
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers.
#endif

static inline
long ov_longmul(long a, long b, int * ov) {

#if __has_builtin(__builtin_mul_overflow)
  long r = LONG_MIN;
  *ov = *ov || __builtin_mul_overflow(a, b, &r);
  return r;
#else
  /* Propagate overflow indicator */
  if (*ov) return LONG_MIN;

  if (a > 0) {  /* a is positive */
    if (b > 0) {  /* a and b are positive */
      if (a > (LONG_MAX / b)) {
        /* Handle error */
	*ov = 1;
	return LONG_MIN;
      }
    } else { /* a positive, b nonpositive */
      if (b < (LONG_MIN / a)) {
        /* Handle error */
	*ov = 1;
	return LONG_MIN;
      }
    } /* a positive, b nonpositive */
  } else { /* a is nonpositive */
    if (b > 0) { /* a is nonpositive, b is positive */
      if (a < (LONG_MIN / b)) {
        /* Handle error */
	*ov = 1;
	return LONG_MIN;
      }
    } else { /* a and b are nonpositive */
      if ( (a != 0) && (b < (LONG_MAX / a))) {
        /* Handle error */
	*ov = 1;
	return LONG_MIN;
      }
    } /* End if a and b are nonpositive */
  } /* End if a is nonpositive */

  return a * b;
#endif
}

static inline
long ov_longadd(long a, long b, int * ov) {

#if __has_builtin(__builtin_add_overflow)
  long r = LONG_MIN;
  *ov = *ov || __builtin_add_overflow(a, b, &r);
  return r;
#else
  /* Propagate overflow indicator */
  if (*ov) return LONG_MIN;

  if (((b > 0) && (a > (LONG_MAX - b))) ||
      ((b < 0) && (a < (LONG_MIN - b)))) {
    /* Handle error */
    *ov = 1;
    return LONG_MIN;

  } else {
    return a + b;
  }
#endif
}

static inline
long ov_longsub(long a, long b, int * ov) {

#if __has_builtin(__builtin_sub_overflow)
  long r = LONG_MIN;
  *ov = *ov || __builtin_sub_overflow(a, b, &r);
  return r;
#else
  /* Propagate overflow indicator */
  if (*ov) return LONG_MIN;

  if ((b > 0 && a < LONG_MIN + b) ||
      (b < 0 && a > LONG_MAX + b)) {
    /* Handle error */
    *ov = 1;
    return LONG_MIN;
  } else {
    return a - b;
  }
#endif
}

static inline
long ov_longdiv(long a, long b, int * ov) {

  /* Propagate overflow indicator */
  if (*ov) return LONG_MIN;

  if ((b == 0) || ((a == LONG_MIN) && (b == -1))) {
    /* Handle error */
    *ov = 1;
    return LONG_MIN;
  } else {
    return a / b;
  }
}

static inline
long ov_longmod(long a, long b, int * ov) {

  /* Propagate overflow indicator */
  if (*ov) return LONG_MIN;

  if ((b == 0 ) || ((a == LONG_MIN) && (b == -1))) {
    /* Handle error */
    *ov = 1;
    return LONG_MIN;
  } else {
    return a % b;
  }
}

static inline
long ov_longneg(long a, int * ov) {
#if __has_builtin(__builtin_sub_overflow)
  long r = LONG_MIN;
  *ov = *ov || __builtin_sub_overflow(0, a, &r);
  return r;
#else
  /* Propagate overflow indicator, and LONG_MIN is overflow on 2's-c. */
  if (*ov || a == LONG_MIN) {
      *ov = 1;
      return LONG_MIN;
  }
  return -a;
#endif
}
