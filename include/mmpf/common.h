
#ifndef MMPF_COMMON_H_INCLUDED
#define MMPF_COMMON_H_INCLUDED

#include "mego/predef/os/windows.h"
#include "mego/predef/symbol/export.h"
#include "mego/predef/symbol/import.h"

#ifndef MMPF_API
#	if defined(MMPF_OPTION__BUILD_SHARED)
#		define MMPF_API MEGO_SYMBOL__EXPORT
#	elif !defined(MMPF_OPTION__BUILD_STATIC)
#		define MMPF_API MEGO_SYMBOL__IMPORT
#	else
#		define MMPF_API
#	endif
#endif

#ifndef MMPF_EXTERN_C
#	ifdef __cplusplus
#		define MMPF_EXTERN_C extern "C"
#	else
#		define MMPF_EXTERN_C
#	endif
#endif

#ifndef MMPF_EXTERN_C_SCOPE_START
#	ifdef __cplusplus
#		define MMPF_EXTERN_C_SCOPE_START extern "C" {
#	else
#		define MMPF_EXTERN_C_SCOPE_START
#	endif
#endif

#ifndef MMPF_EXTERN_C_SCOPE_ENDED
#	ifdef __cplusplus
#		define MMPF_EXTERN_C_SCOPE_ENDED } // extern "C"
#	else
#		define MMPF_EXTERN_C_SCOPE_ENDED
#	endif
#endif

#ifndef MMPF_STDCALL
#	if MEGO_OS__WINDOWS__AVAILABLE
#		define MMPF_STDCALL __stdcall
#	else
#		define MMPF_STDCALL
#	endif
#endif



#endif // !MMPF_COMMON_H_INCLUDED
