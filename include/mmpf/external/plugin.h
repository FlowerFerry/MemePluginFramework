
#ifndef MMPF_EXTERNAL_PLUGIN_H_INCLUDED
#define MMPF_EXTERNAL_PLUGIN_H_INCLUDED

#include "mmpf/common.h"
#include "meme/string_fwd.h"

#include <stdint.h>

#define MMPF_VER_MAJOR (0)
#define MMPF_VER_MINOR (1)
#define MMPF_VER_PATCH (0)

#define MMPF_VER_NUMBER (MEGO__MAKE_VERSION_NUMBER(MMPF_VER_MAJOR, MMPF_VER_MINOR, MMPF_VER_PATCH))
#define MMPF_VER_STRING (MEGO__STRINGIZE(MMPF_VER_MAJOR) "." MEGO__STRINGIZE(MMPF_VER_MINOR) "." MEGO__STRINGIZE(MMPF_VER_PATCH))

MMPF_EXTERN_C_SCOPE_START

typedef MemeInteger_t mmpf_version_t;
typedef MemeInteger_t mmpf_app_version_t;

typedef enum mmpf_ifacelang
{
    mmpf_ifacelang_C
} mmpf_ifacelang_t;

typedef enum mmpf_loglvl
{
    mmpf_loglvl_trace,
    mmpf_loglvl_debug,
    mmpf_loglvl_info,
    mmpf_loglvl_warning,
    mmpf_loglvl_error,
    mmpf_loglvl_fatal
} mmpf_loglvl_t;

typedef struct mmpf_plugin_t
{	
    uint32_t __v0;		//!< Plug-in manager usage
	uint32_t __v1;		//!< Plug-in manager usage
	uint32_t __v2;		//!< Plug-in manager usage
	uint32_t __v3;		//!< Plug-in manager usage
	void * object;
} mmpf_plugin_t;

typedef MemeInteger_t mmpf_obj_preproc_func_t(
    mmpf_plugin_t* _basic, mmint_t _basic_stsize, const MemeByte_t* _app_type, mmint_t _app_type_slen);
typedef mmpf_obj_preproc_func_t * mmpf_obj_preproc_func_ptr;

typedef struct mmpf_obj_params
{
	const MemeByte_t * obj_id;
        mmint_t id_slen;
	mmpf_obj_preproc_func_ptr preproc;
	const struct mmpf_app_services * app_services;
} mmpf_obj_params_t;

typedef struct mmpf_manage_t* mmpf_manage_ptr;
typedef struct mmpf_app_t* mmpf_app_ptr;

typedef void *  mmpf_create_func_t(mmpf_obj_params *, mmint_t _stsize);
typedef mmpf_create_func_t * mmpf_create_func_ptr;

typedef MemeInteger_t mmpf_destroy_func_t(void *);
typedef mmpf_destroy_func_t * mmpf_destroy_func_ptr;


//! @brief The structure filled in by the plugin when registering the object.
//!
//! Register plugin related information to the plugin manager via the callback passed in by the plugin manager
typedef struct mmpf_register_params
{
    const MemeByte_t*  app_type;		//!< 
    mmint_t app_type_slen;				//!<
    mmpf_create_func_ptr  create_func;	//!< Create object callback provided by the plugin
    mmpf_destroy_func_ptr destroy_func;	//!< Destroy object callback provided by the plugin
} mmpf_register_params_t;

typedef MemeInteger_t mmpf_register_obj_func_t(
    mmpf_manage_ptr, const MemeByte_t * _obj_id, 
    mmint_t _id_slen, const struct mmpf_register_params *, mmint_t _stsize);
typedef mmpf_register_obj_func_t * mmpf_register_obj_func_ptr;

//! @brief The plugin provides its own information to the manager
typedef struct mmpf_build_info
{
    mmpf_version_t version;				//!< Plugin framework version used
    mmpf_ifacelang ifacelang;
    const MemeByte_t * build_date;		//!< Usually use __DATE__ to fill
    mmint_t build_date_slen;
    const MemeByte_t * build_time;		//!< Usually use __TIME__ to fill
    mmint_t build_time_slen;
} mmpf_build_info_t;

typedef MemeInteger_t mmpf_register_info_func_t(mmpf_manage_ptr, const mmpf_build_info_t *, mmint_t _stsize);
typedef mmpf_register_info_func_t * mmpf_register_info_func_ptr;

typedef MemeInteger_t mmpf_app_invoke_func_t(
    mmpf_app_ptr, const MemeByte_t * _service_name, mmint_t _name_slen, void * _service_params);
typedef mmpf_app_invoke_func_t * mmpf_app_invoke_func_ptr;

typedef MemeInteger_t mmpf_manager_invoke_func_t(
    mmpf_manage_ptr, const MemeByte_t * _service_name, mmint_t _name_slen, void * _service_params);
typedef mmpf_manager_invoke_func_t * mmpf_manager_invoke_func_ptr;

typedef void mmpf_applog_func_t(mmpf_app_ptr, mmpf_loglvl_t, const MemeByte_t* _msg, mmint_t _msglen);
typedef mmpf_applog_func_t* mmpf_applog_func_ptr;


//! @brief Information provided by the manager
typedef struct mmpf_app_services
{
	mmpf_app_ptr app;
	mmpf_app_version_t version;                 
	mmpf_app_invoke_func_ptr invoke_service;	//!< Invoke service callback provided by the manager
	mmpf_applog_func_ptr log_func;              //!< Log callback provided by the manager

} mmpf_app_services_t;

typedef void mmpf_log_func_t(mmpf_manage_ptr, mmpf_loglvl_t, const uint8_t* _msg, mmint_t _msglen);
typedef mmpf_log_func_t* mmpf_log_func_ptr;

typedef struct mmpf_manage_services {

	mmpf_manage_ptr manage;
	mmpf_version_t version;					        //!< The version used by the platform framework
	mmpf_manager_invoke_func_ptr invoke_service;	//!< Current version not used
	mmpf_log_func_ptr log_func;

} mmpf_manage_services_t;

//! @brief The structure passed by the manager for use by the plugin
typedef struct mmpf_init_params
{
	mmpf_register_obj_func_ptr register_object;		    //!< Register object callback provided by the manager
	mmpf_register_info_func_ptr register_plugin_info;	//!< Register plugin information callback provided by the manager
	const mmpf_manage_services * manage_services;		//!< Manager provides its own information
} mmpf_init_params_t;

typedef MemeInteger_t mmpf_exit_func_t();
typedef mmpf_exit_func_t * mmpf_exit_func_ptr;

typedef MemeInteger_t mmpf_init_func_t(const mmpf_init_params_t*, mmint_t, mmpf_exit_func_ptr *);
typedef mmpf_init_func_t * mmpf_init_func_ptr;

#ifndef MMPF_API
#	if !defined(MMPF_IMPORTS)
#		define MMPF_API MMPF_EXTERN_C MMPF_SYMBOL_EXPORT
#	else
#		define MMPF_API MMPF_EXTERN_C MMPF_SYMBOL_IMPORT
#	endif
#endif

//! @brief Plugin loading entrance
//! @param[in] _params Callbacks and information passed in by the manager
//! @param[in] _stsize mmpf_init_params Structure size, For compatibility
//! @param[out] _func Callback returned by the plugin, Called when the manager uninstalls the plugin
//! 
//! The function of the same name must be implemented by the plugin. 
//! The manager will look for the function with the same name when loading the dynamic library.
MMPF_API
MemeInteger_t mmpf_init(const mmpf_init_params * _params, mmint_t _stsize, mmpf_exit_func_ptr * _func);

MMPF_EXTERN_C_SCOPE_ENDED

#endif // !MMPF_EXTERNAL_PLUGIN_H_INCLUDED
