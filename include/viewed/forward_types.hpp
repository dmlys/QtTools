#pragma once

namespace viewed
{
	/// result of updating filter: should view be completely re-filtered,  incrementally, or not at all
	enum class refilter_type : unsigned
	{
		same, incremental, full
	};


	/// special tag type/value, indicating that sorting should be disabled
	struct nosort_type {} constexpr nosort {};


	/// null sorter for views
	struct null_sorter
	{
		template <class Type1, class Type2>
		bool operator()(const Type1 & v1, const Type2 & v2) const noexcept { return &v1 < &v2; }

		explicit operator bool() const noexcept { return false; }
	};

	/// null filter for views
	struct null_filter
	{
		template <class Type>
		bool operator()(const Type & v) const noexcept { return true; }

		explicit operator bool() const noexcept { return false; }
	};
}
