
#ifndef MMPFPP_PLUGINFRAMEWORK_OUTSIDE_PLUGIN_REGISTRAR_H_INCLUDED
#define MMPFPP_PLUGINFRAMEWORK_OUTSIDE_PLUGIN_REGISTRAR_H_INCLUDED

#include "mmpf/external/plugin.h"
#include "memepp/string_view.hpp"
#include <mego/err/ec.h>

namespace mmpfpp {
namespace external {

	struct plugin_registrar
	{
		plugin_registrar(const struct mmpf_init_params * _params, rsize_t _stsize)
			: params_(_params), params_struct_size_(_stsize)
		{
		}

		inline MemeInteger_t init()
		{
			if (!params_ || sizeof(mmpf_init_params) > params_struct_size_)
				return (MGEC__INVAL);

			mmpf_build_info_t info;
			info.ifacelang = mmpf_ifacelang_C;
			info.build_time = reinterpret_cast<const uint8_t*>(__TIME__);
			info.build_time_slen = sizeof(__TIME__)-1;
			info.build_date = reinterpret_cast<const uint8_t*>(__DATE__);
			info.build_date_slen = sizeof(__DATE__)-1;
			info.version = MMPF_VER_NUMBER;

			if (!params_->register_plugin_info || !params_->manage_services)
				return (MGEC__CANCELED);

			return params_->register_plugin_info(params_->manage_services->manage, &info, sizeof(info));
		}

		// _InterfaceHelper mush has <static void * create_func(mmpf_obj_params *, rsize_t _stsize)>
		// _InterfaceHelper mush has <static MemeInteger_t destroy_func(void *)>
		template<typename _InterfaceHelper>
		inline MemeInteger_t register_object(const memepp::string_view& _name, const MemeByte_t* _apptype, rsize_t _apptype_slen)
		{
			if (!params_ || sizeof(mmpf_init_params) > params_struct_size_)
				return (MGEC__INVAL);

			mmpf_register_params rp;
			rp.app_type = _apptype;
			rp.app_type_slen = _apptype_slen;
			rp.create_func  = _InterfaceHelper::create_func;
			rp.destroy_func = _InterfaceHelper::destroy_func;

			if (!params_->register_object || !params_->manage_services)
				return (MGEC__CANCELED);

			return params_->register_object(params_->manage_services->manage, 
				reinterpret_cast<const uint8_t*>(_name.data()), _name.size(), &rp, sizeof(rp));
		}

	private:
		const struct mmpf_init_params * params_;
		rsize_t params_struct_size_;
	};

};
}; // namespace mmpf

#endif // !MMPFPP_PLUGINFRAMEWORK_OUTSIDE_PLUGIN_REGISTRAR_H_INCLUDED
