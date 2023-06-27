
#ifndef MMPFPP_INTERNAL_OBJECT_ADAPTER_H_INCLUDED
#define MMPFPP_INTERNAL_OBJECT_ADAPTER_H_INCLUDED

#include "mmpfpp/internal/iplugin.h"
#include <memepp/string.hpp>

#include <functional>

namespace mmpfpp {
namespace internal {

	struct iobject_adapter
	{

		virtual ~iobject_adapter() {}

		virtual struct iplugin * create_adapt(void * _object, const std::function<mmpf_destroy_func_t> & _dfp) = 0;

		virtual memepp::string app_type() const = 0;

		inline virtual MemeInteger_t destroy_adapt(iplugin * _p)
		{
			if (_p)
				delete _p;
			return 0;
		}

	};

	template<typename T, typename U>
	struct object_adapter : public iobject_adapter
	{
        typedef T derive_adapter_t;

		virtual struct iplugin * create_adapt(void * _object, const std::function<mmpf_destroy_func_t> & _dfp)
		{
			return new T((U *)_object, _dfp);
		}
	};

}; // namespace internal
}; // namespace mmpfpp

#define MMPF_ADAPTERCLASS_MEMBER(ADAPTER, SRC) \
public: \
	ADAPTER(SRC* _obj, const std::function<mmpf_destroy_func_t>& _fn): \
		obj_(_obj), \
		func_(_fn) \
	{} \
	\
	virtual ~ADAPTER() { \
		if (func_) \
			func_(obj_); \
	} \
private: \
	SRC* obj_; \
	std::function<mmpf_destroy_func_t> func_;

#endif // !MMPFPP_INTERNAL_OBJECT_ADAPTER_H_INCLUDED
