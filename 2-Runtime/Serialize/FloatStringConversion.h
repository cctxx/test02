#if UNITY_EDITOR

/// Converts float/double to and from strings.

/// Binary exact float<-> string conversion functions.
/// Supports the full range of all float values, including nan, inf and denormalized values.

bool FloatToStringAccurate (float f, char* buffer, size_t maximumSize);
bool DoubleToStringAccurate (double f, char* buffer, size_t maximumSize);

bool FloatToStringAccurate (float f, UnityStr& output);
bool DoubleToStringAccurate (double f, UnityStr& output);

float StringToFloatAccurate (const char* buffer);
double StringToDoubleAccurate (const char* buffer);
#endif