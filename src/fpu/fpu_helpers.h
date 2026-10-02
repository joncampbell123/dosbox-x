#ifndef DOSBOX_FPU_HELPERS_H
#define DOSBOX_FPU_HELPERS_H

namespace fpu_detail {

enum class InputClass {
    Unsupported,
    NaN,
    Normal,
    Infinity,
    Zero,
    Denormal,
};

void CheckException();
void SetStatusFromHostExceptions();
uint16_t GetTag();
InputClass ClassifyInput(int op);
bool StackValid(int pos);
void SetQNaN(int pos);
void SetInfinity(int pos, bool negative);
bool InputIsInfinity(int op);
bool InputIsNegative(int op);
bool InputIsNaN(int op);
bool InputIsZero(int op);
void CheckInputDenormals(int op);
void CheckInputDenormals(int op1, int op2);
bool CheckInputs(int op);
bool CheckInputs(int op1, int op2, bool propagate_nan = true);
void PartialRemainder(double& dividend, double divisor);
#ifdef HAS_LONG_DOUBLE
void PartialRemainder(long double& dividend, long double divisor);
#endif
void RaiseLoadExceptions(bool denormal, bool signaling_nan);
void Compare(int op1, int op2, bool ordered);
void CompareToCpuFlags(int op1, int op2, bool ordered);

} // namespace fpu_detail

#endif
