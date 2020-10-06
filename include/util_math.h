
//! \brief Get value sign, i.e. less than zero: -1, zero: 0, greater than zero: 1.
template <typename T> static inline int sgn(const T val) {
	// http://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
	return (T(0) < val) - (val < T(0));
}

//! \brief Floating-point vector with 2 components.
struct DOSBox_Vector2
{
	float X, Y;

	DOSBox_Vector2(const float x, const float y) : X(x), Y(y) { }
	DOSBox_Vector2() : X(0.0f), Y(0.0f) { }

	DOSBox_Vector2 clamp(const DOSBox_Vector2 min, const DOSBox_Vector2 max) const {
		float x = this->X;
		float y = this->Y;
		float xmin = min.X;
		float xmax = max.X;
		float ymin = min.Y;
		float ymax = max.Y;
		x = x < xmin ? xmin : x > xmax ? xmax : x;
		y = y < ymin ? ymin : y > ymax ? ymax : y;
		DOSBox_Vector2 clamp = DOSBox_Vector2(x, y);
		return clamp;
	}

	inline float magnitude(void) const {
		return sqrtf(sqrMagnitude());
	}

	inline float sqrMagnitude(void) const {
		return X * X + Y * Y;
	}

	DOSBox_Vector2 normalized(void) const {
		float m = this->magnitude();
		return m > 0.0f ? DOSBox_Vector2(this->X / m, this->Y / m) : DOSBox_Vector2();
	}

	DOSBox_Vector2 operator*(const float f) const {
		return DOSBox_Vector2(this->X * f, this->Y * f);
	}
};

