#pragma once
#include <algorithm>
#include <varalgo/std_variant_traits.hpp>

#include <boost/range.hpp>
#include <boost/range/detail/range_return.hpp>
#include <boost/algorithm/cxx14/mismatch.hpp>

namespace varalgo
{
	template <class InputIterator1, class InputIterator2>
	struct mismatch_visitor_full :
		boost::static_visitor<std::pair<InputIterator1, InputIterator2>>
	{
		InputIterator1 first1, last1;
		InputIterator2 first2, last2;

		mismatch_visitor_full(InputIterator1 first1, InputIterator1 last1,
		                      InputIterator2 first2, InputIterator2 last2)
			: first1(first1), last1(last1), first2(first2), last2(last2) {}

		template <class Pred>
		inline std::pair<InputIterator1, InputIterator2> operator()(Pred pred) const
		{
			return boost::algorithm::mismatch(first1, last1, first2, last2, pred);
		}
	};

	template <class InputIterator1, class InputIterator2>
	struct mismatch_visitor_half :
		boost::static_visitor<std::pair<InputIterator1, InputIterator2>>
	{
		InputIterator1 first1, last1;
		InputIterator2 first2;

		mismatch_visitor_half(InputIterator1 first1, InputIterator1 last1,
		                      InputIterator2 first2)
			: first1(first1), last1(last1), first2(first2) {}

		template <class Pred>
		inline std::pair<InputIterator1, InputIterator2> operator()(Pred pred) const
		{
			return std::mismatch(first1, last1, first2, pred);
		}
	};

	template <class InputIterator1, class InputIterator2, class Pred>
	inline std::pair<InputIterator1, InputIterator2>
		mismatch(InputIterator1 first1, InputIterator1 last1,
		         InputIterator2 first2, InputIterator2 last2,
		         Pred && pred)
	{
		auto alg = [&first1, &last1, &first2, &last2](auto && pred)
		{
			return std::mismatch(first1, last1, first2, last2, std::forward<decltype(pred)>(pred));
		};

		return variant_traits<std::decay_t<Pred>>::visit(std::move(alg), std::forward<Pred>(pred));
	}

	template <class InputIterator1, class InputIterator2, class Pred>
	inline std::pair<InputIterator1, InputIterator2>
		mismatch(InputIterator1 first1, InputIterator1 last1,
		         InputIterator2 first2,
		         Pred && pred)
	{
		auto alg = [&first1, &last1, &first2](auto && pred)
		{
			return std::mismatch(first1, last1, first2, std::forward<decltype(pred)>(pred));
		};

		return variant_traits<std::decay_t<Pred>>::visit(std::move(alg), std::forward<Pred>(pred));
	}

	template <class SinglePassRange1, class SinglePassRange2, class Pred>
	inline auto mismatch(SinglePassRange1 && rng1, SinglePassRange2 && rng2, Pred && pred)
	{
		return varalgo::mismatch(
			boost::begin(rng1), boost::end(rng1),
			boost::begin(rng2), boost::end(rng2),
			std::forward<Pred>(pred));
	}
}
