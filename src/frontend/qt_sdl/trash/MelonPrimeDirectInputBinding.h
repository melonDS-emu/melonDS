// MelonPrimeDirectInputBinding.h
#pragma once
#ifdef _WIN32
class MelonPrimeDirectInputFilter;
void MelonPrime_BindMetroidHotkeysFromJoystickConfig(MelonPrimeDirectInputFilter* din, int instance);
#endif
