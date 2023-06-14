
#ifndef MMPFPP_INTERNAL_IPLUGIN_H_INCLUDED
#define MMPFPP_INTERNAL_IPLUGIN_H_INCLUDED

#include "mmpf/external/plugin.h"
#include "mego/predef/os/windows.h"
#include "memepp/string_view.hpp"

namespace mmpfpp {
namespace internal {

	inline memepp::string_view dylib_name_suffix()
	{
#ifdef MEGO_OS__WINDOWS__AVAILABLE
		static const char suffix[] = "dll";
#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
		static const char suffix[] = "dylib";
#elif defined(linux) || defined(__linux) || defined(__linux__)
		static const char suffix[] = "so";
#else
#	error "[dylib_name_suffix] function not implemented for this platform"
#endif
		return suffix;
	}

	struct iplugin
	{
		virtual ~iplugin() {}
	};

}; // namespace internal
}; // namespace mmpfpp

#endif // !MMPFPP_INTERNAL_IPLUGIN_H_INCLUDED
