
#ifndef MMPFPP_INTERNAL_MANAGER_H_INCLUDED
#define MMPFPP_INTERNAL_MANAGER_H_INCLUDED

#include <mmpf/external/plugin.h>
#include "object_adapter.h"
#include "ref_counter.h"

#include <map>
#include <tuple>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <functional>

#include <mego/predef/symbol/likely.h>
#include <megopp/os/dynamic_library.h>
#include <megopp/util/scope_cleanup.h>
#include <memepp/string.hpp>
#include <memepp/string_view.hpp>
#include <memepp/convert/std/string.hpp>
#include <memepp/hash/std_hash.hpp>
#include <megopp/util/scope_cleanup.h>
#include <megopp/util/template_random.h>

namespace mmpfpp {
namespace internal {

	namespace mm = memepp;
	namespace mg = megopp;

	inline MemeInteger_t default_plugin_fill_func(
		mmpf_plugin_t* _object, rsize_t _struct_size, const MemeByte_t* _app_type, rsize_t _app_type_slen)
	{
		if (MEGO_SYMBOL__UNLIKELY(!_object || sizeof(mmpf_plugin_t) > _struct_size))
			return MMENO__POSIX_OFFSET(EINVAL);

		_object->__v0 = mg::util::template_random<mg::util::template_seed(__DATE__)>::value;
		_object->__v1 = uint32_t(std::hash<mm::string_view>()(mm_view(_app_type, _app_type_slen)));
		_object->__v2 = uint32_t(_app_type_slen);
		_object->__v3 = 0;
		return 0;
	}

	inline MemeInteger_t default_plugin_valid_func(
		mmpf_plugin_t* _object, const MemeByte_t* _app_type, rsize_t _app_type_slen)
	{
		if (!_object)
			return -1;
		if (_object->__v0 != mg::util::template_random<mg::util::template_seed(__DATE__)>::value)
			return -1;
		if (_object->__v1 != uint32_t(std::hash<mm::string_view>()(mm_view(_app_type, _app_type_slen ))))
			return -1;
		if (_object->__v2 != uint32_t(_app_type_slen))
			return -1;
		if (_object->__v3 != 0)
			return -1;
		return 0;
	}

	typedef MemeInteger_t plugin_valid_func_t(
		mmpf_plugin_t* _object, const MemeByte_t* _app_type, rsize_t _app_type_slen);
	typedef plugin_valid_func_t* plugin_valid_func_ptr;

	struct app_service_adapter
	{
		app_service_adapter() 
		{
			memset(&application_services_, 0, sizeof(application_services_));
			application_services_.app = reinterpret_cast<mmpf_app_ptr>(this);
			application_services_.log_func = __provide_log_callback;
			application_services_.invoke_service = __provide_invoke_callback;
		}

		virtual ~app_service_adapter() {}

		inline mmpf_app_version_t version() const
		{
			return application_services_.version;
		}

		inline void version(mmpf_app_version_t _version)
		{
			application_services_.version = _version;
		}

		inline mmpf_app_services& application_services()
		{
			return application_services_;
		}

	private:
		
		inline void __provide_log(mmpf_loglvl_t, const mm::string_view&);
		inline MemeInteger_t __provide_invoke(const mm::string_view& _name, void *  _param);

		inline static void __provide_log_callback(
			mmpf_app_ptr, mmpf_loglvl_t, const uint8_t* _msg, rsize_t _msglen
		);
		inline static MemeInteger_t __provide_invoke_callback(
			mmpf_app_ptr, const uint8_t * _service_name, rsize_t _namelen, void * _service_params
		);

		std::function<void(mmpf_loglvl_t, const mm::string_view&)> log_fn_;
		std::function<void(const mm::string_view& _name, void* _param)> invoke_fn_;
		mmpf_app_services application_services_;
	};

	inline void app_service_adapter::__provide_log(mmpf_loglvl_t _lvl, const mm::string_view & _msg)
	{
		try {
			if (log_fn_)
				log_fn_(_lvl, _msg);
		}
		catch (...) {
			// TODO: log
		}
	}

	inline MemeInteger_t app_service_adapter::__provide_invoke(const mm::string_view & _name, void * _param)
	{
		mm::string msg;
		try {
			if (invoke_fn_)
				invoke_fn_(_name, _param);
			return 0;
		}
		catch (std::exception& ex) {
			msg = ex.what();
		}
		if (msg.size()) {
			__provide_log(mmpf_loglvl_error,
				mm::c_format(256, "(throw exception: %s)", msg.data()));
			return -1;
		}
		return 0;
	}

	inline void app_service_adapter::__provide_log_callback(
		mmpf_app_ptr _app, mmpf_loglvl_t _lvl, const uint8_t * _msg, rsize_t _msglen)
	{
		if (!_app)
			return;

		auto app = reinterpret_cast<app_service_adapter*>(_app);
		app->__provide_log(_lvl, mm_view(_msg, _msglen));
	}

	inline MemeInteger_t app_service_adapter::__provide_invoke_callback(
		mmpf_app_ptr _app, const uint8_t * _service_name, rsize_t _namelen, void * _service_params)
	{
		if (!_app || !_service_name)
			return MMENO__POSIX_OFFSET(EINVAL);

		auto app = reinterpret_cast<app_service_adapter*>(_app);
		return app->__provide_invoke(mm_view(_service_name, _namelen), _service_params);
	}

	class manager;
	struct mmpf_manage_t
	{
		manager* this_;
		mm::string plugin_id_;
	};

	struct __object_parameter
	{
		mm::string id_;
		mm::string app_type_;
	};
    typedef std::shared_ptr<__object_parameter> __object_param_ptr;

	struct __object_instance
	{
		mm::string id_;
		std::function<mmpf_create_func_t > cfunc_;
		std::function<mmpf_destroy_func_t> dfunc_;
		std::shared_ptr<mg::os::dynamic_library> dylib_;
	};
    typedef std::shared_ptr<__object_instance> __object_instance_ptr;

	enum class plugin_status_t
	{
		unknown,
		loading,
		loaded,
		unloading,
		unloaded
	};

	struct __plugin_parameter
	{
		typedef mm::string_view object_id_view_t;
		typedef std::unordered_map<object_id_view_t, __object_param_ptr > objects_t;

        __plugin_parameter() :
            status_(plugin_status_t::unloaded)
        {}

		mm::string id_;
		mm::string path_;
		mm::string builddate_;
		mm::string buildtime_;
		mmpf_version_t version_;
		mmpf_ifacelang ifacelang_;

        objects_t objects_;

		plugin_status_t status_;
	};
    typedef std::shared_ptr<__plugin_parameter> __plugin_param_ptr;

	struct __plugin_instance
	{
        typedef mm::string_view object_id_view_t;
		typedef std::unordered_map<object_id_view_t, __object_instance_ptr > objects_t;
		
		__plugin_instance():
			ref_counter_(0)
		{}

		~__plugin_instance() 
		{
			if (exit_func_)
				exit_func_();
		}

		mm::string id_;
		ref_counter<std::mutex> ref_counter_;
		std::shared_ptr<mg::os::dynamic_library> dylib_;
		std::function<mmpf_exit_func_t> exit_func_;

        objects_t objects_;
	};
    typedef std::shared_ptr<__plugin_instance> __plugin_instance_ptr;

	struct __user_handle
	{	
		typedef std::function<void(const mm::string_view& _plugin_id, 
			const mm::string_view& _object_id)> uninstall_callback;
		typedef uninstall_callback objloaded_callback;

		std::function<void(const mm::string_view& _plugin_id,
				const mm::string_view& _object_id)> loaded_func_;
		std::function<void(const mm::string_view& _plugin_id,
				const mm::string_view& _object_id)> destroy_func_;
		
		std::unordered_map<mm::string_view, objloaded_callback> objloaded_callbacks_;
		std::unordered_map<mm::string_view, uninstall_callback> uninstall_callbacks_;
	};
	using __user_handle_ptr = std::shared_ptr<__user_handle>;
	
	class manager : public std::enable_shared_from_this<manager>
	{
	public:
		typedef MemeInteger_t integer_t;
		typedef MemeInteger_t errno_t;
        typedef mm::string plugin_id_t;
        typedef mm::string object_id_t;
        typedef mm::string_view plugin_id_view_t;
        typedef mm::string_view object_id_view_t;
		
		typedef std::function<void(mmpf_loglvl_t, const mm::string_view&)> log_callback;
		typedef std::function<void(const mm::string_view& _plugin_id, 
			const mm::string_view& _object_id)> uninstall_callback;
		typedef uninstall_callback objloaded_callback;

		typedef std::unordered_map<plugin_id_view_t, __plugin_param_ptr > plugin_params_t;
		typedef std::unordered_map<object_id_view_t, __object_param_ptr > object_params_t;

		typedef std::unordered_map<mm::string_view, __user_handle_ptr> user_handles_t;

		typedef std::unordered_map<plugin_id_view_t,
			__plugin_instance_ptr > plugin_instances_t;
		typedef std::unordered_map<object_id_view_t,
			__object_instance_ptr > object_instances_t;

		manager();
//		~manager();

		inline void set_log_callback(log_callback _fn);
		
		errno_t init_plugin (const mm::string & _plugin_id, mmpf_init_func_ptr _func);
		errno_t load_by_path(const mm::string & _plugin_id, const mm::string & _path);
		errno_t unload(plugin_id_view_t);

        errno_t resume(plugin_id_view_t);
		errno_t try_pause_idle();
		errno_t try_pause(plugin_id_view_t);
		

		inline void register_uninstall_callback(mm::string_view, mm::string_view, uninstall_callback _fn);
		inline void register_objloaded_callback(mm::string_view, mm::string_view, objloaded_callback _fn);

		integer_t create_object_ptr(
			const mm::string_view & _object_id,
			const mm::string_view & _plugin_id,
			app_service_adapter & _app_service,
			iobject_adapter & _adapter,
			iplugin* & _object
		);

		template<typename __object_factory>
		std::tuple<errno_t, std::shared_ptr<typename __object_factory::derive_adapter_t>>
			create_object(
				const mm::string_view& _object_id,
				const mm::string_view& _plugin_id,
				app_service_adapter& _app_service,
				__object_factory& _factory);
	private:

		inline void __deref_plugin(const std::shared_ptr<__plugin_instance>& _inst);

		inline errno_t __remove_plugin(const mm::string_view& _plugin_id);

        __object_param_ptr __find_object_param_sync(
			const plugin_id_view_t& _plugin_id, const object_id_view_t& _object_id);
        __object_instance_ptr __find_object_instance_sync(
            const plugin_id_view_t& _plugin_id, const object_id_view_t& _object_id);

		inline void __log(mmpf_loglvl_t, const mm::string_view&);
		
		inline void __provide_log(
			const mm::string_view& _plugin_id, mmpf_loglvl_t, const mm::string_view&);
		inline integer_t __provide_invoke(
			const mm::string_view& _plugin_id, const mm::string_view& _service_name, void * _service_params
		);
		inline integer_t __provide_register_plugin_info(
			const mm::string_view& _plugin_id, const mmpf_build_info_t *, rsize_t _struct_size
		);
		inline integer_t __provide_register_object(
			const mm::string_view& _plugin_id, const mm::string_view& _object_id,
			const mmpf_register_params * _params, rsize_t _struct_size
		);

		inline static void __provide_log_callback(
			mmpf_manage_ptr, mmpf_loglvl_t, const uint8_t* _msg, rsize_t _msglen
		);
		inline static integer_t __provide_invoke_callback(
			mmpf_manage_ptr, const uint8_t * _service_name, rsize_t _namelen, void * _service_params
		);
		inline static integer_t __provide_register_object_callback(
			mmpf_manage_ptr, const uint8_t * _object_id, rsize_t _id_strlen,
			const mmpf_register_params * _params, rsize_t _struct_size
		);
		inline static integer_t __provide_register_plugin_info_callback(
			mmpf_manage_ptr, const mmpf_build_info_t *, rsize_t _struct_size
		);

		mm::string init_func_name_;
		mmpf_obj_preproc_func_ptr plugin_fill_fn_;
		plugin_valid_func_ptr plugin_valid_fn_;
		log_callback log_fn_;

		mutable std::mutex mtx_;
		plugin_params_t plugin_params_;
		object_params_t object_params_;
		plugin_instances_t  plugin_insts_;
		object_instances_t  object_insts_;

		user_handles_t user_handles_;
	};

	inline manager::manager():
		init_func_name_("mmpf_init"),
		plugin_fill_fn_ (default_plugin_fill_func),
		plugin_valid_fn_(default_plugin_valid_func)
	{
	}

	inline void manager::set_log_callback(log_callback _fn)
	{
		log_fn_.swap(_fn);
	}

	inline manager::errno_t manager::init_plugin(const mm::string & _plugin_id, mmpf_init_func_ptr _func)
	{
		do {
			std::unique_lock<std::mutex> locker(mtx_);
			auto it = plugin_params_.find((_plugin_id));
			if (it == plugin_params_.end())
			{
				auto pp = std::make_shared<__plugin_parameter>();
				pp->status_ = plugin_status_t::loading;
				pp->id_ = _plugin_id;
				plugin_params_.emplace(_plugin_id.to_large(), pp);
			}
			
		} while (0);

		auto errorCleanup = mg::util::scope_cleanup__create(
			[this, &_plugin_id] { __remove_plugin(_plugin_id); });
		
		do {
			std::unique_lock<std::mutex> locker(mtx_);
			auto piit = plugin_insts_.find(_plugin_id);
			if (piit == plugin_insts_.end()) {
				// static registration

				auto pi = std::make_shared<__plugin_instance>();
				pi->id_ = _plugin_id;
				piit = plugin_insts_.insert(std::make_pair(pi->id_, pi)).first;

			}
		} while (0);

		mmpf_manage_t provide_manage_object;
		mmpf_manage_services_t provide_manage_services;
		mmpf_init_params_t provide_init_params;

		provide_manage_object.this_ = this;
		provide_manage_object.plugin_id_ = _plugin_id;

		provide_manage_services.manage = reinterpret_cast<mmpf_manage_ptr>(&provide_manage_object);
		provide_manage_services.version = MMPF_VER_NUMBER;
		provide_manage_services.log_func = __provide_log_callback;
		provide_manage_services.invoke_service = __provide_invoke_callback;

		provide_init_params.manage_services = &provide_manage_services;
		provide_init_params.register_object = __provide_register_object_callback;
		provide_init_params.register_plugin_info = __provide_register_plugin_info_callback;

		mmpf_exit_func_ptr efunc = NULL;
		MemeInteger_t ret = 0;
		try {
			ret = _func(&provide_init_params, sizeof(provide_init_params), &efunc);
		}
		catch (...) {
			__log(mmpf_loglvl_error, mm::c_format(256,
				"plugin initialization function throws an unkown exception and plugin id(%s)", _plugin_id.data()));
			return -1;
		}
		if (ret) {
			__log(mmpf_loglvl_error, mm::c_format(256,
				"plugin initialization function failed and plugin id(%s), errcode(%d)", _plugin_id.data(), int(ret)));
			return MMENO__POSIX_OFFSET(EOTHER);
		}
		if (!efunc) {
			__log(mmpf_loglvl_error, mm::c_format(256,
				"the plugin initialization process did not find the exit function and plugin id(%s)", _plugin_id.data()));
			return -1;
		}
		
        __plugin_parameter::objects_t object_params;
		do {
			std::unique_lock<std::mutex> locker(mtx_);
			auto piit = plugin_insts_.find(_plugin_id);
			if (piit == plugin_insts_.end()) {
                __log(mmpf_loglvl_error, mm::c_format(256,
                    "the plugin initialization process did not find the plugin instance and plugin id(%s)", _plugin_id.data()));
                return -1;
			}
			piit->second->exit_func_ = efunc;

            auto ppit = plugin_params_.find(_plugin_id);
            if (ppit == plugin_params_.end()) {
                __log(mmpf_loglvl_error, mm::c_format(256,
                    "the plugin initialization process did not find the plugin parameter and plugin id(%s)", _plugin_id.data()));
                return -1;
            }

			if (!(ppit->second->objects_.empty())) 
			{
				object_params = ppit->second->objects_;
				break;
			}
			
			locker.unlock();
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"The plugin(%s) has not successfully registered any objects", _plugin_id.data()));
			return MMENO__POSIX_OFFSET(ECANCELED);
		} while (0);
		errorCleanup.cancel();

		for (auto& it : object_params) {
			__user_handle_ptr uh;
			do {
				std::unique_lock<std::mutex> locker(mtx_);
				auto hit = user_handles_.find(it.second->app_type_);
				if (hit == user_handles_.end())
					continue;
				uh = hit->second;
			} while (0);

			if (!uh)
				break;

			for (auto& cb_it : uh->objloaded_callbacks_) {
				try {
					if (cb_it.second)
						cb_it.second(_plugin_id, it.first);
				}
				catch (std::exception & ex) {
					__log(mmpf_loglvl_warning, mm::c_format(256,
						"Throw an exception when executing a loaded callback. desc(%s), callback name(%s)",
						ex.what(), cb_it.second.target_type().name()));
				}
			}
			
		}

		do {
			std::unique_lock<std::mutex> locker(mtx_);
			auto pit = plugin_params_.find(_plugin_id);
            if (pit == plugin_params_.end())
                break;
            pit->second->status_ = plugin_status_t::loaded;
		} while (0);
		return 0;
	}

	inline manager::errno_t manager::load_by_path(const mm::string & _plugin_id, const mm::string & _path)
	{
		do {
			std::unique_lock<std::mutex> locker(mtx_);
			auto it = plugin_params_.find(_plugin_id);
			if (it == plugin_params_.end())
				break;

			locker.unlock();
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"Register the same ID(%s) repeatedly", _plugin_id.data()));
			return MMENO__POSIX_OFFSET(EEXIST);
		} while (0);

		mm::string err;
		auto dylib = mg::os::dynamic_library::load(_path, err, true);
		if (!dylib) {
			__log(mmpf_loglvl_error, mm::c_format(256,
				"Load dynamic library failed, msg(%s), file path(%s)",
				err.data(), _path.data()));
			return -1;
		}

		auto func = (dylib->get_symbol<mmpf_init_func_t>(init_func_name_.data()));
		if (!func) {
			__log(mmpf_loglvl_error, mm::c_format(256,
				"Dynamic library without the necessary initialization function 'plugin initialize'"
				"file(%s)", _path.data()));
			return -1;
		}

		std::unique_lock<std::mutex> locker(mtx_);
		auto pi = std::make_shared<__plugin_instance>();
		pi->id_    = _plugin_id;
		pi->dylib_ = dylib;
		plugin_insts_.insert(std::make_pair(pi->id_, pi));
		locker.unlock();
		auto pluginInstCleanup = mg::util::scope_cleanup__create([&] 
		{
			locker.lock();
			plugin_insts_.erase(_plugin_id);
		});

		auto ret = init_plugin(_plugin_id, func);
		if ( ret < 0 ) {
			__log(mmpf_loglvl_error, mm::c_format(256,
				"Initialize plugin error. file(%s)", _path.data()));
			return ret;
		}

		pluginInstCleanup.cancel();

		locker.lock();
		auto & params = plugin_params_[_plugin_id];
		params->path_ = _path;
		return 0;
	}
	
	inline manager::errno_t manager::unload(plugin_id_view_t _id)
	{
        auto ec = __remove_plugin(_id);
		if (ec == MMENO__POSIX_OFFSET(EBUSY)) {
            return __remove_plugin(_id);
		}
		return ec;
	}
	
	inline manager::errno_t manager::resume(plugin_id_view_t _id)
	{
        std::unique_lock<std::mutex> locker(mtx_);
        auto iit = plugin_insts_.find(_id);
		if (iit != plugin_insts_.end())
			return 0;
        auto pit = plugin_params_.find(_id);
        if (pit == plugin_params_.end())
            return MMENO__POSIX_OFFSET(ENOENT);
        auto& params = pit->second;
        locker.unlock();
		
        mm::string err;
        auto dylib = mg::os::dynamic_library::load(params->path_, err, true);
        if (!dylib) {
            __log(mmpf_loglvl_error, mm::c_format(256,
                "Load dynamic library failed, msg(%s), file path(%s)",
                err.data(), params->path_.data()));
            return MMENO__POSIX_OFFSET(ENOENT);
        }
		
        auto func = (dylib->get_symbol<mmpf_init_func_t>(init_func_name_.data()));
        if (!func) {
            __log(mmpf_loglvl_error, mm::c_format(256,
                "Dynamic library without the necessary initialization function; "
                "file(%s)", params->path_.data()));
            return MMENO__POSIX_OFFSET(ENOENT);
        }
		
        auto pi = std::make_shared<__plugin_instance>();
        pi->id_ = _id.to_string();
        pi->dylib_ = dylib;
        plugin_insts_.insert(std::make_pair(pi->id_, pi));

        auto pluginInstCleanup = mg::util::scope_cleanup__create([&]
        {
            locker.lock();
            plugin_insts_.erase(_id);
        });

        auto ret = init_plugin(pi->id_, func);
        if (ret < 0) {
            __log(mmpf_loglvl_error, mm::c_format(256,
                "Initialize plugin error. file(%s)", params->path_.data()));
            return ret;
        }
		
        pluginInstCleanup.cancel();
        return 0;
	}
	
	inline manager::errno_t manager::try_pause_idle()
	{
        std::unique_lock<std::mutex> locker(mtx_);
        if (plugin_insts_.empty())
            return 0;
        auto instances = plugin_insts_;
        locker.unlock();
        for (auto& it : instances) {
			if (!it.second->dylib_) {
                // static inline plugin
				continue;
			}

			if (it.second->ref_counter_.is_meet(locker))
			{
				try {
					auto ret = it.second->exit_func_();
					if (ret) {
                        __log(mmpf_loglvl_error, mm::c_format(256,
                            "Plugin exit failed; plugin id(%s)", it.first.data())); 
					}
					locker.lock();
					for (auto& o : it.second->objects_) {
						auto oiit = object_insts_.find(o.first);
						if (oiit != object_insts_.end())
						{
							if (oiit->second == o.second)
								object_insts_.erase(oiit);
						}
					}
					plugin_insts_.erase(it.first);
					locker.unlock();
				}
				catch (std::exception& ex) {
                    __log(mmpf_loglvl_error, mm::c_format(256,
                        "Throw an exception when executing a exit callback; desc(%s), callback name(%s)",
                        ex.what(), it.second->exit_func_.target_type().name()));
				}
				catch (...) {
                    __log(mmpf_loglvl_error, mm::c_format(256,
                        "Throw an unknown exception when executing a exit callback; callback name(%s)",
                        it.second->exit_func_.target_type().name()));
				}
				
			}
        }
		return 0;
	}

	inline manager::errno_t manager::try_pause(plugin_id_view_t _id)
	{
        std::unique_lock<std::mutex> locker(mtx_);
        auto it = plugin_insts_.find(_id);
        if (it == plugin_insts_.end())
            return 0;
        auto pi = it->second;

		if (!pi->dylib_) {
            // static inline plugin
            return 0;
        }
		
        if (pi->ref_counter_.is_meet(locker))
        {
			locker.unlock();
            try {
                auto ret = pi->exit_func_();
                if (ret) {
                    __log(mmpf_loglvl_error, mm::c_format(256,
                        "Plugin exit failed; plugin id(%s)", _id.data()));
                }
				
                locker.lock();
                for (auto& o : pi->objects_) {
                    auto oiit = object_insts_.find(o.first);
                    if (oiit != object_insts_.end()) 
					{
                        if (oiit->second == o.second)
                            object_insts_.erase(oiit);
                    }
                }
                plugin_insts_.erase(_id);
            }
            catch (std::exception& ex) {
                __log(mmpf_loglvl_error, mm::c_format(256,
                    "Throw an exception when executing a exit callback; desc(%s), callback name(%s)",
                    ex.what(), pi->exit_func_.target_type().name()));
            }
            catch (...) {
                __log(mmpf_loglvl_error, mm::c_format(256,
                    "Throw an unknown exception when executing a exit callback; callback name(%s)",
                    pi->exit_func_.target_type().name()));
            }
			return 0;
        }
		else {
            return MMENO__POSIX_OFFSET(EBUSY);
		}
	}

	inline void manager::register_uninstall_callback(mm::string_view _caller, mm::string_view _type, uninstall_callback _fn)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		auto it = user_handles_.find(_type);
		__user_handle_ptr uh;
		if (it == user_handles_.end()) {
			uh = std::make_shared<__user_handle>();
		}
		else {
			uh = std::make_shared<__user_handle>(*it->second);
		}
		uh->uninstall_callbacks_[mm::string{ _caller.data(), _caller.size(), mm::string_storage_t::large }] = _fn;
		user_handles_[mm::string{ _type.data(), _type.size(), mm::string_storage_t::large }] = uh;
	}

	inline void manager::register_objloaded_callback(mm::string_view _caller, mm::string_view _type, objloaded_callback _fn)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		auto it = user_handles_.find(_type);
		__user_handle_ptr uh;
		if (it == user_handles_.end()) {
			uh = std::make_shared<__user_handle>();
		}
		else {
			uh = std::make_shared<__user_handle>(*it->second);
		}
		uh->objloaded_callbacks_[mm::string{ _caller.data(), _caller.size(), mm::string_storage_t::large }] = _fn;
		user_handles_[mm::string{ _type.data(), _type.size(), mm::string_storage_t::large }] = uh;
	}

	inline manager::errno_t manager::create_object_ptr(
		const mm::string_view & _object_id,
		const mm::string_view & _plugin_id,
		app_service_adapter & _app_service,
		iobject_adapter & _adapter, iplugin* & _object
	)
	{
		_object = nullptr;

		std::unique_lock<std::mutex> locker(mtx_);
		
        auto objparam = __find_object_param_sync(_plugin_id, _object_id);
        if (!objparam) {
            locker.unlock();
            __log(mmpf_loglvl_warning, mm::c_format(256,
                "The plugin(%s) object(%s) was not found",
                _plugin_id.to_string().data(), _object_id.to_string().data()));
            return -1;
        }
		
		auto paramit = plugin_params_.find(_plugin_id.to_string());
		if (paramit == plugin_params_.end()) {
			locker.unlock();
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"The plugin(%s) object(%s) was not found",
				_plugin_id.to_string().data(), _object_id.to_string().data()));
			return -1;
		}

        if (paramit->second->status_ != plugin_status_t::loaded) {
            locker.unlock();
            __log(mmpf_loglvl_warning, mm::c_format(256,
                "The plugin(%s) object(%s) was not found",
                _plugin_id.to_string().data(), _object_id.to_string().data()));
            return -1;
        }
		
		std::shared_ptr<__plugin_instance> pluinst;
		auto pluit = plugin_insts_.find(_plugin_id.to_string());
		if (pluit != plugin_insts_.end())
			pluinst = pluit->second;
		
		if (!pluinst) {
            locker.unlock();
			// If it is a statically registered plug-in, it will not be executed here

            auto ec = resume(_plugin_id);
            if (ec) {
                __log(mmpf_loglvl_warning, mm::c_format(256,
                    "The plugin(%s) object(%s) was not found",
                    _plugin_id.to_string().data(), _object_id.to_string().data()));
                return ec;
            }
            locker.lock();
            pluit = plugin_insts_.find(_plugin_id.to_string());
            if (pluit != plugin_insts_.end())
                pluinst = pluit->second;
		}
		if (pluinst == nullptr)
		{
			locker.unlock();
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"The plug-in(%s) was not found",
				_plugin_id.to_string().data()));
			return -1;
		}
		pluinst->ref_counter_.increment(locker);
		locker.unlock();
		auto decrementCleanup = megopp::util::scope_cleanup__create([&] { 
			pluinst->ref_counter_.decrement(locker); 
		});

		if (_adapter.app_type() != objparam->app_type_)
		{
			__log(mmpf_loglvl_warning, "The requested application type is inconsistent");
			return -1;
		}

		auto objinst = __find_object_instance_sync(_plugin_id, _object_id);
		if (!objinst) {
			locker.unlock();
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"The plugin(%s) object(%s) was not found",
				_plugin_id.to_string().data(), _object_id.to_string().data()));
			return -1;
		}

		mmpf_obj_params_t object_params;

		object_params.obj_id  = _object_id.bytes();
		object_params.id_slen = _object_id.size();
		object_params.preproc = plugin_fill_fn_;
		object_params.app_services = &(_app_service.application_services());

		void * object = nullptr;
		try {
			object = objinst->cfunc_(&object_params, sizeof(mmpf_obj_params_t)
			);
		}
		catch (...) {
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"Throw an exception when creating an plugin(%s) object(%s)",
				_plugin_id.to_string().data(), _object_id.to_string().data()));
			return -1;
		}
		if (!object) {
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"The plug-in(%s) object(%s) creation failed",
				_plugin_id.to_string().data(), _object_id.to_string().data()));
			return -1;
		}
		auto pointer = reinterpret_cast<mmpf_plugin_t*>(object);

		if (plugin_valid_fn_(pointer, _adapter.app_type().bytes(), _adapter.app_type().size()) != 0)
		{
			__log(mmpf_loglvl_warning, mm::c_format(256,
				"Plugin(%s) object(%s) verification failed",
				_plugin_id.to_string().data(), _object_id.to_string().data()));
			return -1;
		}

		std::function<mmpf_destroy_func_t> destroy_catch_func = 
			[dfunc = objinst->dfunc_, pluinst, w = weak_from_this()](void* _p)->integer_t
		{
			MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] 
			{
				auto m = w.lock();
				if (m)
					m->__deref_plugin(pluinst);
			});

			try {
				return dfunc(_p);
			}
			catch (...) {
                // TO_DO
				return -1;
			}
		};

		_object = (_adapter.create_adapt(object, destroy_catch_func));
		decrementCleanup.cancel();
		return 0;
	}

	template<typename __object_factory>
	inline std::tuple<manager::errno_t, std::shared_ptr<typename __object_factory::derive_adapter_t>>
		manager::create_object(
			const mm::string_view& _object_id,
			const mm::string_view& _plugin_id,
			app_service_adapter& _app_service,
            __object_factory& _factory)
	{
        iplugin* plug_ptr = nullptr;
        auto result = create_object_ptr(_object_id, _plugin_id, _app_service, _factory, plug_ptr);
        if (result < 0)
			return std::make_tuple(result, 
				std::shared_ptr<typename __object_factory::derive_adapter_t>{});
		
        if (!plug_ptr)
            return std::make_tuple(errno_t(-1),
				std::shared_ptr<typename __object_factory::derive_adapter_t>{});
		
        auto object_cleanup = megopp::util::scope_cleanup__create([&] {
            _factory.destroy_adapt(plug_ptr);
        });
		
        auto object = dynamic_cast<typename __object_factory::derive_adapter_t*>(plug_ptr);
        if (!object)
            return std::make_tuple(errno_t(-1),
				std::shared_ptr<typename __object_factory::derive_adapter_t>{});

        auto sp = std::shared_ptr<typename __object_factory::derive_adapter_t>(
            object, [](typename __object_factory::derive_adapter_t* _p) {
			__object_factory::instance().destroy_adapt(_p);
        });
		
        object_cleanup.cancel();
        return std::make_tuple(errno_t(0), sp);
	}

	inline void manager::__deref_plugin(const std::shared_ptr<__plugin_instance>& _inst)
	{
		_inst->ref_counter_.decrement(mtx_);
	}

	inline manager::errno_t manager::__remove_plugin(const plugin_id_view_t & _id)
	{
		std::unique_lock<std::mutex> locker(mtx_);
		auto pit = plugin_params_.find(_id);
		if (pit == plugin_params_.end()) {
            return MMENO__POSIX_OFFSET(ENOENT);
		}
        auto pluparam = pit->second;
		pluparam->status_ = plugin_status_t::unloading;
		
		auto iit = plugin_insts_.find(_id);
		if (iit == plugin_insts_.end())
		{
			pluparam->status_ = plugin_status_t::unloaded;
			plugin_params_.erase(_id);
			return 0;
		}

        auto pluinst   = iit->second;
		auto objParams = pluparam->objects_;
		locker.unlock();
		
		for (auto& o : objParams) {
            auto objparam = o.second;
            if (!objparam)
                continue;

			__user_handle_ptr uh;
			do {
				std::unique_lock<std::mutex> uh_locker(mtx_);
				auto hit = user_handles_.find(objparam->app_type_);
				if (hit == user_handles_.end())
					continue;
				uh = hit->second;
			} while (0);
			
            if (!uh)
                continue;
			
            for (auto it : uh->uninstall_callbacks_) 
			{
                try {
                    it.second(_id, o.first);
                }
				catch (std::exception& ex) {
					
				}
                catch (...) {
                    // TO_DO
                }
            }

			std::unique_lock<std::mutex> op_locker(mtx_);
            auto opit = object_params_.find(o.first);
            if (opit != object_params_.end())
			{
				if (opit->second == o.second)
					object_params_.erase(opit);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (pluinst->ref_counter_.is_meet(locker))
		{
            auto ec = try_pause(_id);
            if (ec != 0)
                return ec;
		}
		else {
			return MMENO__POSIX_OFFSET(EBUSY);
		}
		
		locker.lock();
		pluparam->status_ = plugin_status_t::unloaded;
        plugin_insts_.erase (_id);
		plugin_params_.erase(_id);
        return 0;
	}

	inline __object_param_ptr manager::__find_object_param_sync(
		const plugin_id_view_t& _plugin_id, const object_id_view_t& _object_id)
	{
        auto ppit = plugin_params_.find(_plugin_id);
        if (ppit == plugin_params_.end())
            return nullptr;
        
		auto opit = ppit->second->objects_.find(_object_id);
        if (opit == ppit->second->objects_.end())
            return nullptr;
		
        return opit->second;
	}
	
	inline __object_instance_ptr manager::__find_object_instance_sync(
		const plugin_id_view_t& _plugin_id, const object_id_view_t& _object_id)
	{
        auto piit = plugin_insts_.find(_plugin_id);
        if (piit == plugin_insts_.end())
            return nullptr;
		
        auto oiit = piit->second->objects_.find(_object_id);
        if (oiit == piit->second->objects_.end())
            return nullptr;

        return oiit->second;
	}

	inline void manager::__log(mmpf_loglvl_t _lvl, const mm::string_view & _sv)
	{
		try {
			if (log_fn_)
				log_fn_(_lvl, _sv);
		}
		catch (...) {
		}
	}

	inline void manager::__provide_log(
		const mm::string_view & _plugin_id, mmpf_loglvl_t _lvl, const mm::string_view & _sv)
	{
		__log(_lvl, mm::c_format(256, "[%s] %s", 
			_plugin_id.to_string().data(), _sv.to_string().data()));
	}

	inline manager::integer_t manager::__provide_invoke(
		const mm::string_view & _plugin_id, const mm::string_view & _service_name, void * _service_params)
	{
		return 0;
	}

	inline void manager::__provide_log_callback(
		mmpf_manage_ptr _manage, mmpf_loglvl_t _lvl, const uint8_t * _msg, rsize_t _msglen)
	{
		if (!_manage)
			return;
		auto manage = reinterpret_cast<mmpf_manage_t*>(_manage);
		if (!manage->this_)
			return;
		manage->this_->__provide_log(manage->plugin_id_, _lvl, mm_view(_msg , _msglen));
	}

	inline manager::integer_t manager::__provide_invoke_callback(
		mmpf_manage_ptr _manage, const uint8_t * _service_name, rsize_t _namelen, void * _service_params)
	{
		if (!_manage)
			return MMENO__POSIX_OFFSET(EINVAL);
		auto manage = reinterpret_cast<mmpf_manage_t*>(_manage);
		if (!manage->this_)
			return MMENO__POSIX_OFFSET(EINVAL);

		return manage->this_->__provide_invoke(
			manage->plugin_id_, mm_view(_service_name, _namelen), _service_params);
	}

	inline manager::integer_t manager::__provide_register_object_callback(
		mmpf_manage_ptr _manage, const uint8_t * _object_id, rsize_t _id_strlen,
		const mmpf_register_params_t * _params, rsize_t _struct_size
	)
	{
		if (!_manage)
			return MMENO__POSIX_OFFSET(EINVAL);
		auto manage = reinterpret_cast<mmpf_manage_t*>(_manage);
		if (!manage->this_)
			return MMENO__POSIX_OFFSET(EINVAL);

		return manage->this_->__provide_register_object(
			manage->plugin_id_, mm_view(_object_id, _id_strlen), _params, _struct_size
		);
	}

	inline manager::integer_t manager::__provide_register_plugin_info_callback(
		mmpf_manage_ptr _manage, const mmpf_build_info_t * _info, rsize_t _struct_size)
	{
		if (!_manage)
			return MMENO__POSIX_OFFSET(EINVAL);
		auto manage = reinterpret_cast<mmpf_manage_t*>(_manage);
		if (!manage->this_)
			return MMENO__POSIX_OFFSET(EINVAL);

		return manage->this_->__provide_register_plugin_info(
			manage->plugin_id_, _info, _struct_size);
	}

	inline manager::integer_t manager::__provide_register_plugin_info(
		const mm::string_view & _plugin_id, const mmpf_build_info_t* _info, rsize_t _struct_size)
	{
		if (_plugin_id.empty() || !_info)
			return MMENO__POSIX_OFFSET(EINVAL);
		if (sizeof(mmpf_build_info_t) > _struct_size)
			return MMENO__POSIX_OFFSET(EINVAL);

        mmpf_version_t version = MMPF_VER_NUMBER;
		if (MEGO__GET_VERSION_MAJOR(_info->version) != MEGO__GET_VERSION_MAJOR(version)
			|| MEGO__GET_VERSION_MINOR(_info->version) > MEGO__GET_VERSION_MINOR(version))
		{
			__log(mmpf_loglvl_error, mm::c_format(256,
				"Plugin uses framework version incompatibility, "
				"plugin use framework version(%lld), current framework version(%lld), plugin id(%s)",
				int64_t(_info->version), int64_t(version), _plugin_id.to_string().data()
			));
			return -1;
		}

		std::lock_guard<std::mutex> locker(mtx_);
		auto&params = plugin_params_[_plugin_id.to_string()];

		params->ifacelang_ = _info->ifacelang;
		params->builddate_ = _info->build_date ? mm_from(_info->build_date, _info->build_date_slen ) : "";
		params->buildtime_ = _info->build_time ? mm_from(_info->build_time, _info->build_time_slen ) : "";
		params->version_ = _info->version;
		return 0;
	}
	
	inline manager::integer_t manager::__provide_register_object(
		const mm::string_view & _plugin_id, const mm::string_view & _object_id, 
		const mmpf_register_params_t * _params, rsize_t _struct_size
	)
	{
		if (_plugin_id.empty() || _object_id.empty())
			return MMENO__POSIX_OFFSET(EINVAL);
		if (!_params || sizeof(mmpf_register_params_t) > _struct_size)
			return MMENO__POSIX_OFFSET(EINVAL);

		try {
			if (!_params->create_func || !_params->destroy_func)
			{
				__log(mmpf_loglvl_warning, mm::c_format(256,
					"The object(%s) creation function or destruction function registered by the plugin(%s) is empty",
					_object_id.to_string().data(), _plugin_id.to_string().data()));
				return MMENO__POSIX_OFFSET(EINVAL);
			}

			std::unique_lock<std::mutex> locker(mtx_);
            auto objparam = __find_object_param_sync   (_plugin_id, _object_id);
            auto objinst  = __find_object_instance_sync(_plugin_id, _object_id);
			if (!objparam) {
                objparam = std::make_shared<__object_parameter>();
                objparam->id_ = _object_id.to_string();
                object_params_.insert(std::make_pair(objparam->id_, objparam));
				
				auto ppit = plugin_params_.find(_plugin_id);
				if (ppit != plugin_params_.end())
					ppit->second->objects_.insert(std::make_pair(objparam->id_, objparam));
			}

            if (!objinst) {
                objinst = std::make_shared<__object_instance>();
                objinst->id_ = _object_id.to_string();
                object_insts_.insert(std::make_pair(objinst->id_, objinst));
				
                auto piit = plugin_insts_.find(_plugin_id);
                if (piit != plugin_insts_.end())
                    piit->second->objects_.insert(std::make_pair(objinst->id_, objinst));
            }
			
			objinst->cfunc_		= _params->create_func;
			objinst->dfunc_		= _params->destroy_func;
			objparam->app_type_ = _params->app_type;

			auto piit = plugin_insts_.find(_plugin_id);
			if (piit != plugin_insts_.end())
				objinst->dylib_ = piit->second->dylib_;
			
		}
		catch (std::exception & ex) {
            // TODO
			return -1;
		}
		catch (...) {
            // TODO
			return -1;
		}
		return 0;
	}

}; // namespace internal
}; // namespace mmpfpp

#endif // !MMPFPP_INTERNAL_MANAGER_H_INCLUDED
