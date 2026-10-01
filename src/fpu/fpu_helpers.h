#ifndef DOSBOX_FPU_HELPERS_H
#define DOSBOX_FPU_HELPERS_H

namespace fpu_detail {

void CheckException();
void SetStatusFromHostExceptions();
void SetInfinity(int op, bool negative);
bool StackValid(int pos);
bool InputIsInfinity(int op);
bool InputIsNegative(int op);
bool InputIsNaN(int op);
bool InputIsZero(int op);
void CheckInputs(int op);
void CheckInputs(int op1, int op2, bool check_denormal = true);
void RaiseLoadExceptions(bool denormal, bool signaling_nan);
void Compare(int op1, int op2, bool ordered);
void CompareToCpuFlags(int op1, int op2, bool ordered);

} // namespace fpu_detail

#endif
