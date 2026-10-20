// Unreal defines MYSTERYCORE_API as DLLEXPORT/DLLIMPORT when building the module; those come
// from Unreal's platform headers, which this engine-free module does not include, so they are
// supplied here when missing. The standalone test build links everything into one executable
// and leaves MYSTERYCORE_API empty.
#pragma once

#if defined(_MSC_VER)
	#ifndef DLLEXPORT
		#define DLLEXPORT __declspec(dllexport)
	#endif
	#ifndef DLLIMPORT
		#define DLLIMPORT __declspec(dllimport)
	#endif
#else
	#ifndef DLLEXPORT
		#define DLLEXPORT __attribute__((visibility("default")))
	#endif
	#ifndef DLLIMPORT
		#define DLLIMPORT __attribute__((visibility("default")))
	#endif
#endif

#ifndef MYSTERYCORE_API
	#define MYSTERYCORE_API
#endif

#if defined(_MSC_VER)
	// Exported classes hold std:: members. Harmless here: both sides of the DLL boundary are built
	// by the same compiler against the same runtime, which Unreal's module builds guarantee.
	#pragma warning(disable : 4251)
#endif
