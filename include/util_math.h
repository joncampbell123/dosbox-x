
//! \brief Get value sign, i.e. less than zero: -1, zero: 0, greater than zero: 1.
template <typename T> static inline int sgn(const T val) {
	// http://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
	return (T(0) < val) - (val < T(0));
}

