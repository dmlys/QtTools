#pragma once
#include <viewed/sftree_facade_qtbase.hpp>
#include <viewed/sftree_is_base_of.hpp>

namespace viewed
{
	template <class ... Types>
	struct sftree_model_base_type
	{
		static_assert(sizeof...(Types) >= 1, "insufficient template parameters to sftree_model_qtbase");

		using first_type = boost::mp11::mp_first<boost::mp11::mp_list<Types...>>;
		using type = std::conditional_t
		<
		    (sizeof... (Types) > 1 or not sftree_detail::is_base_of<sftree_facade_qtbase, first_type>::value),
		    sftree_facade_qtbase<Types...>,
		    first_type
		>;
	};

	/// Types are:
	/// either Traits + ModelBase, same as in sftree_facade_qtbase
	/// or     sftree_facade_qtbase<Trait, ModelBase> derived class
	template <class ... Types>
	class sftree_model_qtbase : public sftree_model_base_type<Types...>::type
	{
		using self_type = sftree_model_qtbase;
		using base_type = typename sftree_model_base_type<Types...>::type;

	public:
		using typename base_type::leaf_type;
		using typename base_type::node_type;

	protected:
		using typename base_type::ivalue_ptr;
		using typename base_type::page_type;

	protected:
		void fill_children_leafs(const page_type & page, std::vector<const leaf_type *> & elements);

	public:
		template <class Iterator>
		auto assign(Iterator first, Iterator last) -> std::enable_if_t<ext::is_iterator_v<Iterator>>;

		template <class Iterator>
		auto upsert(Iterator first, Iterator last) -> std::enable_if_t<ext::is_iterator_v<Iterator>>;

		template <class Range>
		auto assign(Range && range) -> std::enable_if_t<ext::is_range_v<Range>>;

		template <class Range>
		auto upsert(Range && range) -> std::enable_if_t<ext::is_range_v<Range>>;
		
		void clear();

	public:
		using base_type::base_type;
	};

	template <class ... Types>
	void sftree_model_qtbase<Types...>::fill_children_leafs(const page_type & page, std::vector<const leaf_type *> & elements)
	{
		for (auto & child : page.children)
		{
			if (child.index() == PAGE)
				fill_children_leafs(*static_cast<const page_type *>(child.pointer()), elements);
			else
				elements.push_back(static_cast<const leaf_type *>(child.pointer()));
		}
	}

	template <class ... Types>
	void sftree_model_qtbase<Types...>::clear()
	{
		this->beginResetModel();
		this->m_root.children.clear();
		this->m_root.nvisible = 0;
		this->endResetModel();
	}

	template <class ... Types>
	template <class Range>
	inline std::enable_if_t<ext::is_range_v<Range>>
	sftree_model_qtbase<Types...>::assign(Range && range)
	{
		return assign(boost::begin(range), boost::end(range));
	}

	template <class ... Types>
	template <class Range>
	inline std::enable_if_t<ext::is_range_v<Range>>
	sftree_model_qtbase<Types...>::upsert(Range && range)
	{
		return upsert(boost::begin(range), boost::end(range));
	}


	template <class ... Types>
	template <class Iterator>
	std::enable_if_t<ext::is_iterator_v<Iterator>>
	sftree_model_qtbase<Types...>::assign(Iterator first, Iterator last)
	{
		std::vector<const leaf_type *> existing;
		std::vector<std::unique_ptr<leaf_type>> elements;
		ext::try_reserve(elements, first, last);

		auto make_unique = [](auto && element) { return std::make_unique<leaf_type>(std::forward<decltype(element)>(element)); };
		elements.assign(
		    boost::make_transform_iterator(first, make_unique),
		    boost::make_transform_iterator(last, make_unique));

		fill_children_leafs(this->m_root, existing);

		auto el_first = elements.begin();
		auto el_last  = elements.end();

		auto erased_first = existing.begin();
		auto erased_last  = existing.end();

		this->group_by_paths(erased_first, erased_last);
		this->group_by_paths(el_first, el_last);

		auto leaf_equal = [traits_ptr = &this->m_traits](auto && l1, auto && l2) { return base_type::path_equal_to(traits_ptr->get_path(*l1), traits_ptr->get_path(*l2)); };
		el_last = std::unique(el_first, el_last, leaf_equal);


		auto pred = [this, erased_first, erased_last](auto & ptr)
		{
			// e1 - from [erased_first, erased_last), e2 - from ptr
			auto pred = [this](auto * e1, auto * e2) { return this->path_group_pred(viewed::unmark_pointer(e1), e2); };

			// not (*it < ptr.get())
			// not (ptr.get() < *it) => *it === ptr
			auto it = std::lower_bound(erased_first, erased_last, ptr.get(), pred);
			if (it == erased_last or pred(ptr.get(), viewed::unmark_pointer(*it))) return false;

			*it = viewed::mark_pointer(*it);
			return true;
		};
		
		auto pp = std::stable_partition(el_first, el_last, pred);
		erased_last = std::remove_if(erased_first, erased_last, viewed::marked_pointer);

		// [el_first, pp) - updated, [pp, el_last) - inserted
		auto updated  = boost::make_iterator_range(el_first, pp) | ext::moved;
		auto inserted = boost::make_iterator_range(pp, el_last)  | ext::moved;

		return this->update_data_and_notify(
		    erased_first, erased_last,
		    updated.begin(), updated.end(),
		    inserted.begin(), inserted.end());
	}

	template <class ... Types>
	template <class Iterator>
	std::enable_if_t<ext::is_iterator_v<Iterator>>
	sftree_model_qtbase<Types...>::upsert(Iterator first, Iterator last)
	{
		std::vector<const leaf_type *> existing;
		std::vector<std::unique_ptr<leaf_type>> elements;
		ext::try_reserve(elements, first, last);

		auto make_unique = [](auto && element) { return std::make_unique<leaf_type>(std::forward<decltype(element)>(element)); };
		elements.assign(
		    boost::make_transform_iterator(first, make_unique),
		    boost::make_transform_iterator(last, make_unique));

		fill_children_leafs(this->m_root, existing);

		auto el_first = elements.begin();
		auto el_last  = elements.end();

		auto ex_first = existing.begin();
		auto ex_last  = existing.end();

		this->group_by_paths(ex_first, ex_last);
		this->group_by_paths(el_first, el_last);

		auto leaf_equal = [traits_ptr = &this->m_traits](auto && l1, auto && l2) { return base_type::path_equal_to(traits_ptr->get_path(*l1), traits_ptr->get_path(*l2)); };
		el_last = std::unique(el_first, el_last, leaf_equal);


		auto pred = [this, ex_first, ex_last](auto & ptr)
		{
			// e1 - from [ex_first, ex_last), e2 - from ptr
			auto pred = [this](auto * e1, auto * e2) { return this->path_group_pred(e1, e2); };

			// not (*it < ptr.get())
			// not (ptr.get() < *it) => *it === ptr
			auto it = std::lower_bound(ex_first, ex_last, ptr.get(), pred);
			return it != ex_last and not pred(ptr.get(), *it);
		};

		auto pp = std::stable_partition(el_first, el_last, pred);

		// [el_first, pp) - updated, [pp, el_last) - inserted
		auto updated  = boost::make_iterator_range(el_first, pp) | ext::moved;
		auto inserted = boost::make_iterator_range(pp, el_last)  | ext::moved;

		return this->update_data_and_notify(
		    ex_first, ex_first, // no erases
		    updated.begin(), updated.end(),
		    inserted.begin(), inserted.end());
	}
}
