#pragma once
#include <algorithm>
#include <varalgo/std_variant_traits.hpp>
#include <boost/range.hpp>

namespace varalgo
{
	template <class InputIterator, class OutputIterator, class Pred>
	inline OutputIterator copy_if(InputIterator first, InputIterator last, OutputIterator dest, Pred && pred)
	{
		auto alg = [&first, &last, &dest](auto && pred)
		{
			return std::copy_if(first, last, dest, std::forward<decltype(pred)>(pred));
		};

		return variant_traits<std::decay_t<Pred>>::visit(std::move(alg), std::forward<Pred>(pred));
	}

	/// range overloads
	template <class SinglePassRange, class OutputIterator, class Pred>
	inline OutputIterator copy_if(const SinglePassRange & rng, OutputIterator dest, Pred && pred)
	{
		return varalgo::copy_if(boost::begin(rng), boost::end(rng), std::move(dest), std::forward<Pred>(pred));
	}
}
