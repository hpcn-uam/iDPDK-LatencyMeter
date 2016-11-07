#ifndef __HPTL_CONF__H__
#define __HPTL_CONF__H__

//Basic defines for processing other defines
#define HBC_xstr(a) HBC_str(a)
#define HBC_str(a) #a

//The current version
#define hptl_VERSION_trash v0.9-13-g2765e12

#define hptl_VERSION HBC_xstr(hptl_VERSION_trash)

//Is hptl debug on?
#define HPTL_DEBUG

#define HPTL_TSC
#define HPTL_CLOCKREALTIME

#endif
