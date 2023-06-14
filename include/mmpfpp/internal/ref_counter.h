
#ifndef MMPFPP_INTERNAL_REF_COUNTER_H_INCLUDED
#define MMPFPP_INTERNAL_REF_COUNTER_H_INCLUDED

#include <megopp/help/null_mutex.h>
#include <megopp/util/scope_cleanup.h>

#include <memory>
#include <functional>
#include <mutex>

namespace mmpfpp {
namespace internal {

	template<typename _Mtx = megopp::auxiliary::null_mutex>
	struct ref_counter
	{
		ref_counter() : count_(0), compare_value_(0) {}
		ref_counter(MemeInteger_t _count):
			count_(_count)
		{}

        inline void set_callback(const std::function<void(const ref_counter&)>& _cb)
        {
            if (_cb) {
                cb_ = std::make_shared< std::function<void(const ref_counter&)> >(_cb);
            }
        }
		
		inline ref_counter& increment(_Mtx& _mtx)
		{
			std::unique_lock<_Mtx> locker(_mtx);
			++count_;
			
			return *this;
		}

		inline ref_counter& increment(std::unique_lock<_Mtx>& _locker)
		{
			auto owns = _locker.owns_lock();
			auto cleanup = megopp::util::scope_cleanup__create([&]
			{
				if (owns && owns != _locker.owns_lock())
					_locker.lock();
			});

			if (!owns)
				_locker.lock();

			++count_;
			
			return *this;
		}

		inline ref_counter& decrement(_Mtx& _mtx)
		{
			std::unique_lock<_Mtx> locker(_mtx);
            auto count = count_--;

			if (count > compare_value_ && count_ == compare_value_) 
			{
				locker.unlock();

				if (cb_)
					(*cb_)(*this);
			}
			return *this;
		}

		inline ref_counter& decrement(std::unique_lock<_Mtx>& _locker)
		{
			auto owns = _locker.owns_lock();
			auto cleanup = megopp::util::scope_cleanup__create([&]
			{
				if (owns)
					_locker.lock();
			});

			if (!owns)
				_locker.lock();

			auto count = count_--;

			if (count > compare_value_ && count_ == compare_value_) 
			{
				_locker.unlock();

				if (cb_)
					(*cb_)(*this);
			}
			return *this;
		}

        inline constexpr bool is_meet(std::unique_lock<_Mtx>& _locker) const noexcept
        {
			auto owns = _locker.owns_lock();
            auto cleanup = megopp::util::scope_cleanup__create([&]
            {
                if (owns)
                    _locker.lock();
            });
            if (!owns)
                _locker.lock();
			
			return count_ <= compare_value_;
        }

		MemeInteger_t count_;
		MemeInteger_t compare_value_;
		std::shared_ptr< std::function<void(const ref_counter&)> > cb_;
	};

};
}; // namespace mmpfpp

#endif // !MMPFPP_INTERNAL_REF_COUNTER_H_INCLUDED
