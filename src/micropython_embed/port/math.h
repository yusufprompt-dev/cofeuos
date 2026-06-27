#pragma once
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define isfinite(x) __builtin_isfinite(x)
double floor(double x);
double ceil(double x);
double sqrt(double x);
double pow(double x, double y);
double fmod(double x, double y);
double sin(double x);
double cos(double x);
double tan(double x);
double exp(double x);
double log(double x);
double log2(double x);
double log10(double x);
double fabs(double x);
double round(double x);
double trunc(double x);
