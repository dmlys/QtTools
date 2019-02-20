#pragma once
#include <boost/config.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/signals2.hpp>


namespace viewed
{
	/// Traits class describes signal types used by container and views
	/// container holds and emits signals, view - connects to them and synchronize it's state to container
	/// This is a default interface. Your container's and views can use different signals and signal_range_type if they want,
	/// of course they probably will not be able to use views/containers with different signal interface
	template <class Type>
	struct default_signal_traits
	{
	protected:
		/// right now containers and views are only used from GUI thread - mutex not needed
		typedef boost::signals2::dummy_mutex signal_mutex_type;

	public:
		/// random access range of valid pointers(at least at moment of call) to value_type
		typedef boost::iterator_range<const Type **> signal_range_type;

		inline static signal_range_type make_range(const Type ** first, const Type ** last)
		{ return boost::make_iterator_range(first, last); }


		typedef boost::signals2::connection          connection;
		typedef boost::signals2::scoped_connection   scoped_connection;

		/// signal is emitted in process of updating data in container(after update/insert, before erase) with 3 ranges of pointers.
		///  * 1st to erased elements, sorted by pointer value
		///  * 2nd points to elements that were updated, sorted by pointer value
		///  * 3rd to newly inserted, order is unspecified
		typedef typename boost::signals2::signal_type<
			void(signal_range_type sorted_erased, signal_range_type sorted_updated, signal_range_type inserted),
			boost::signals2::keywords::mutex_type<signal_mutex_type>
		>::type update_signal_type;

		/// signal is emitted before data is erased from container,
		/// with range of pointers to elements to erase, sorted by pointer value
		typedef typename boost::signals2::signal_type<
			void(signal_range_type erased),
			boost::signals2::keywords::mutex_type<signal_mutex_type>
		>::type erase_signal_type;

		/// signal is emitted before container is cleared.
		typedef typename boost::signals2::signal_type<
			void(),
			boost::signals2::keywords::mutex_type<signal_mutex_type>
		>::type clear_signal_type;
	};


	template <class Type, class = void>
	struct is_signal_traits_lookalile : std::false_type {};

	template <class Type>
	struct is_signal_traits_lookalile<Type, std::void_t<
		typename Type::signal_range_type,
		typename Type::update_signal_type,
		typename Type::erase_signal_type,
		typename Type::clear_signal_type
	>> : std::true_type {};

	/// you can specialize this trait to explicitly define if concrete type is a signal_traits type
	template <class Type>
	struct is_signal_traits : is_signal_traits_lookalile<Type> {};

}
