#pragma once
#include <memory>
#include <vector>
#include <tuple>
#include <algorithm>

#include <viewed/algorithm.hpp>
#include <viewed/forward_types.hpp>
#include <viewed/get_functor.hpp>
#include <viewed/indirect_functor.hpp>
#include <viewed/qt_model.hpp>
#include <viewed/pointer_variant.hpp>

#include <varalgo/sorting_algo.hpp>
#include <varalgo/on_sorted_algo.hpp>

#include <ext/config.hpp>
#include <ext/utility.hpp>
#include <ext/try_reserve.hpp>
#include <ext/iterator/zip_iterator.hpp>
#include <ext/iterator/outdirect_iterator.hpp>
#include <ext/iterator/indirect_iterator.hpp>
#include <ext/range/range_traits.hpp>
#include <ext/range/adaptors/moved.hpp>
#include <ext/range/adaptors/outdirected.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtTools/ToolsBase.hpp>

#if BOOST_COMP_GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

namespace viewed
{
	inline namespace sftree_constants
	{
		// NODE and PAGE are same.
		// NODE is public type, while PAGE is internal type

		constexpr unsigned NODE = 0;
		constexpr unsigned PAGE = 0;
		constexpr unsigned LEAF = 1;
	}

	/// sftree_facade_qtbase is a facade for building qt tree models.
	/// It expects source data to be in form of a list and provides hierarchical view on it.
	/// It implements complex stuff:
	/// * internal tree structure;
	/// * sorting/filtering;
	/// * QAbstractItemModel stuff: index calculation, persistent index management on updates and sorting/filtering;
	///
	/// ModelBase - QAbstractItemModel derived base interface/class this facade implements.
	/// Traits    - traits class describing type and how to work with this type.
	///
	/// This class implements rowCount, index, parent methods from QAbstractItemModel; it does not implement columnCount, data, headerData.
	/// It also provides method for:
	///  * getting leaf/node from given QModelIndex
	///  * helper methods for implementation filtering/sorting
	///  * updating, resetting model from new data
	///
	/// Traits should provide next:
	///   using leaf_type = ...
	///    leaf, this is original type, sort of value_type.
	///    It will be provided to this facade in some sort of list and tree hierarchy will be built from it.
	///    It can be simple struct or a more complex class.
	///
	///   using node_type = ...
	///    node, similar to leaf, but can have other nodes/leaf children, can be same leaf, or different type
	///    (it does not hold children directly, sftree_facade_qtbase takes care of that internally).
	///    It will be created internally while calculating tree structure.
	///    sftree_facade_qtbase will provide some methods/hooks to populate/calculate values of this type
	///
	///   using path_type = ...
	///     type representing path, copy-constructable, for example std::string, std::filesystem::path, etc
	///     each leaf holds path in some way: direct member, accessed via some getter, or even leaf itself is a path.
	///     path describes hierarchy structure in some, sufficient, way. For example filesystem path: /segment1/segment2/...
	///
	///   using pathview_type = ...  path view, it's same to path, as std::string_view same to std::string. can be same as path_type.
	///
	///   using path_equal_to_type   = ... predicate for comparing paths
	///   using path_less_type       = ... predicate for comparing paths
	///   using path_hash_type       = ... path hasher
	///     those predicates should be able to handle both path_type and pathview_type:
	///       std::less<>, std::equal_to<> are good candidates
	///       for std::string and std::string_view - you can use std::hash<std::string_view>
	///       for QString and QStringRef - see QtTools/ToolsBase.hpp
	///
	///
	///   NOTE: static members can be functors
	///   methods for working with given types:
	///     static void set_name(node_type & node, pathview_type && path, pathview_type && name);
	///       assigns path and name to node, path is already contains name, no concatenation is needed.
	///       In fact node can hold only given name part, or whole path if desired.
	///       possible implementation: item.name = name
	///
	///     static auto get_name(const leaf_type & leaf) -> path_type/pathview_type/const path_type &;
	///     static auto get_name(const node_type & node) -> path_type/pathview_type/const path_type &;
	///       returns/extracts name from leaf/node, usually something like: return extract_name(leaf/node.name)
	///
	///     static auto get_path(const leaf_type & leaf) -> path_type/pathview_type/const path_type &;
	///       returns whole path from leaf, usually something like: return leaf.filepath
	///
	///                                                                                                PAGE/LEAF     leaf/node name   newcontext
	///     auto parse_path(const pathview_type & path, const pathview_type & context) -> std::tuple<std::uintptr_t, pathview_type, pathview_type>;
	///       parses given path under given context, where:
	///         path    - path from leaf_type acquired via get_path call
	///         context - is a parent path, at start - empty, but then it will be one returned by this function
	///       returns if it's LEAF/PAGE:
	///         if it's PAGE - also returns node name and newcontext/newpath(old path + node name)
	///         if it's LEAF - returns leaf name, value of newcontext is unused
	///
	///     bool is_child(const pathview_type & path, const pathview_type & context, const pathview_type & node_name);
	///       returns if leaf path(acquired via get_path from leaf_type) is a logically a child of node with given node_name and under given context.
	///       Note context is same as returned from parse_path from previous step and does not include node name.
	///
	///
	///   using sort_pred_type = ...
	///   using filter_pred_type = ...
	///     predicates for sorting/filtering leafs/nodes based on some criteria, this is usually sorting by columns and filtering by some text
	///     should be default constructable
	///
	template <class Traits, class ModelBase = QAbstractItemModel>
	class sftree_facade_qtbase :
		public ModelBase,
		protected Traits
	{
		using self_type = sftree_facade_qtbase;
		static_assert(std::is_base_of_v<QAbstractItemModel, ModelBase>);

	protected:
		using model_helper = AbstractItemModel;
		using int_vector = std::vector<int>;
		using int_vector_iterator = int_vector::iterator;

	public:
		using traits_type = Traits;
		using model_base = ModelBase;

		// bring traits types used by this sftree_facade
		using typename traits_type::leaf_type;
		using typename traits_type::node_type;

		using typename traits_type::path_type;
		using typename traits_type::pathview_type;
		using typename traits_type::path_equal_to_type;
		using typename traits_type::path_less_type;
		using typename traits_type::path_hash_type;

		using typename traits_type::sort_pred_type;
		using typename traits_type::filter_pred_type;

	public:
		/// value_ptr is sort of value_type, variant of leaf or node
		//using value_ptr = viewed::pointer_variant<const node_type *, const leaf_type *>;

	protected:
		using traits_type::parse_path;
		using traits_type::is_child;

	protected:
		struct page_type;

		/// internal_value_ptr is variant of page or leaf pointers,
		/// it is what this and derived class can work with, some function will provide values of this type
		using internal_value_ptr  = viewed::pointer_variant<const page_type *, const leaf_type *>;
		/// here and further i stands for internal
		using ivalue_ptr          = internal_value_ptr;
		using ivalue_ptr_vector   = std::vector<const ivalue_ptr *>;
		using ivalue_ptr_iterator = typename ivalue_ptr_vector::iterator;

		struct get_name_type
		{
			using result_type = pathview_type;
			//decltype(auto) operator()(const path_type & path) const { return traits_type::get_name(path); }
			decltype(auto) operator()(const leaf_type & leaf) const { return traits_type::get_name(leaf); }
			decltype(auto) operator()(const page_type & page) const { return traits_type::get_name(page.node); }
			decltype(auto) operator()(const ivalue_ptr & val) const { return viewed::visit(*this, val); }

			// important, viewed::visit(*this, val) depends on them, otherwise infinite recursion would occur
			decltype(auto) operator()(const leaf_type * leaf) const { return traits_type::get_name(*leaf); }
			decltype(auto) operator()(const page_type * page) const { return traits_type::get_name(page->node); }
		};

		struct path_group_pred_type
		{
			// arguments - swapped, intended, sort in descending order
			bool operator()(const leaf_type & l1, const leaf_type & l2) const noexcept { return path_less(traits_type::get_path(l2),  traits_type::get_path(l1));  }
			//bool operator()(const leaf_type * l1, const leaf_type * l2) const noexcept { return path_less(traits_type::get_path(*l2), traits_type::get_path(*l1)); }

			template <class Pointer1, class Pointer2>
			std::enable_if_t<
					std::is_same_v<leaf_type, ext::remove_cvref_t<decltype(*std::declval<Pointer1>())>>
				and std::is_same_v<leaf_type, ext::remove_cvref_t<decltype(*std::declval<Pointer2>())>>
				, bool
			>
			operator()(Pointer1 && p1, Pointer2 && p2) const noexcept { return path_less(traits_type::get_path(*p2), traits_type::get_path(*p1)); }
		};

		using ivalue_container = boost::multi_index_container<
			ivalue_ptr,
			boost::multi_index::indexed_by<
				boost::multi_index::hashed_unique<get_name_type, path_hash_type, path_equal_to_type>,
				boost::multi_index::random_access<>
			>
		>;

		static constexpr unsigned by_code = 0;
		static constexpr unsigned by_seq  = 1;

		using code_view_type = typename ivalue_container::template nth_index<by_code>::type;
		using seq_view_type  = typename ivalue_container::template nth_index<by_seq> ::type;

		// Leafs are provided from external sources, some form of assign method implemented by derived class
		// nodes are created by this class, while node itself is defined by traits and calculated by derived class,
		// we add children/parent management part, also filtering is supported.
		//
		// Because pages are created by us - we manage their life-time
		// and when some children nodes/pages are filtered out we can't just delete them - later we would forced to recalculate them, which is unwanted.
		// Instead we define visible part and shadow part.
		// Children container is partitioned is such way that first comes visible elements, after them shadowed - those who does not pass filter criteria.
		// Whenever filter criteria changes, or elements are changed - elements are moved to/from shadow/visible part according to changes.

		struct page_type
		{
			page_type *      parent = nullptr; // our parent
			std::size_t      nvisible = 0;     // number of visible elements in container, see above
			ivalue_container children;         // our children
			node_type        node;             // node data
		};

		struct get_children_type
		{
			using result_type = const ivalue_container &;
			result_type operator()(const leaf_type & leaf) const { return ms_empty_container; }
			result_type operator()(const page_type & page) const { return page.children; }
			result_type operator()(const ivalue_ptr & val) const { return viewed::visit(*this, val); }

			// important, viewed::visit(*this, val) depends on this, otherwise infinite recursion would occur
			template <class Type>
			result_type operator()(const Type * ptr) const { return operator()(*ptr); }
		};

		struct get_children_count_type
		{
			using result_type = std::size_t;
			result_type operator()(const leaf_type & leaf) const { return 0; }
			result_type operator()(const page_type & page) const { return page.nvisible; }
			result_type operator()(const ivalue_ptr & val) const { return viewed::visit(*this, val); }

			// important, viewed::visit(*this, val) depends on this, otherwise infinite recursion would occur
			template <class Type>
			result_type operator()(const Type * ptr) const { return operator()(*ptr); }
		};

	protected:
		/// context used for recursive tree resorting
		struct resort_context
		{
			ivalue_ptr_vector * vptr_array;                                      // helper value_ptr ptr vector, reused to minimize heap allocations
			int_vector * index_array, * inverse_array;                           // helper index vectors, reused to minimize heap allocations
			QModelIndexList::const_iterator model_index_first, model_index_last; // persistent indexes that should be recalculated
		};

		/// context used for recursive tree refiltering
		struct refilter_context
		{
			ivalue_ptr_vector * vptr_array;                                      // helper value_ptr ptr vector, reused to minimize heap allocations
			int_vector * index_array, *inverse_array;                            // helper index vectors, reused to minimize heap allocations
			QModelIndexList::const_iterator model_index_first, model_index_last; // persistent indexes that should be recalculated
		};

		/// context used for recursive processing of inserted, updated, erased elements
		template <class ErasedRandomAccessIterator, class UpdatedRandomAccessIterator, class InsertedRandomAccessIterator>
		struct update_context_template
		{
			// those members are intensely used by update_page_and_notify, rearrange_children_and_notify, process_erased, process_updated, process_inserted methods

			// all should be groped by group_by_paths
			ErasedRandomAccessIterator erased_first, erased_last;
			UpdatedRandomAccessIterator updated_first, updated_last;
			InsertedRandomAccessIterator inserted_first, inserted_last;

			int_vector_iterator
				removed_first, removed_last, // share same array, append by incrementing removed_last
				changed_first, changed_last; //                   append by decrementing changed_first

			std::ptrdiff_t inserted_diff, updated_diff, erased_diff;
			std::size_t inserted_count, updated_count, erased_count;

			pathview_type path;
			pathview_type inserted_path, updated_path, erased_path;
			pathview_type inserted_name, updated_name, erased_name;

			ivalue_ptr_vector * vptr_array;
			int_vector * index_array, *inverse_array;
			QModelIndexList::const_iterator model_index_first, model_index_last;
		};

		template <class RandomAccessIterator>
		struct reset_context_template
		{
			RandomAccessIterator first, last;
			pathview_type path;
			ivalue_ptr_vector * vptr_array;
		};

	protected:
		// allows uniform access to leaf/node, see more description below at node_accessor_type definition
		struct node_accessor_type;

		// filter, sort helpers. defined below, outside of class
		template <class Pred> struct ivalue_ptr_filter_type;
		template <class Pred> struct ivalue_ptr_sorter_type;

	protected:
		// those are sort of member functions, but functors
		static constexpr path_equal_to_type path_equal_to {};
		static constexpr path_less_type     path_less {};
		static constexpr path_hash_type     path_hash {};

		static constexpr get_name_type           get_name {};
		static constexpr get_children_type       get_children {};
		static constexpr get_children_count_type get_children_count {};
		static constexpr path_group_pred_type    path_group_pred {};

		static constexpr node_accessor_type node_accessor {};
		static constexpr auto make_ref = [](auto * ptr) { return std::ref(*ptr); };

	protected:
		static const pathview_type     ms_empty_path;
		static const ivalue_container  ms_empty_container;


	protected:
		// root page, note it's somewhat special, it's parent node is always nullptr,
		// and node_type part is empty and unused
		page_type m_root;

		sort_pred_type m_sort_pred;
		filter_pred_type m_filter_pred;


	protected:
		template <class Functor>
		static void for_each_child_page(page_type & page, Functor && func);

		template <class RandomAccessIterator>
		static void group_by_paths(RandomAccessIterator first, RandomAccessIterator last);

	protected: // core QAbstractItemModel functionality implementation
		/// creates index for element with row, column in given page, this is just more typed version of QAbstractItemModel::createIndex
		QModelIndex create_index(int row, int column, page_type * ptr) const;
		/// returns page holding leaf/node pointed by index(or just parent node in other words)
		page_type * get_page(const QModelIndex & index) const;
		/// returns leaf/page pointed by index
		const ivalue_ptr & get_ielement_ptr(const QModelIndex & index) const;

	public:
		/// find element by given path starting from root elements, if no such element is found - invalid index returned
		virtual QModelIndex find_element(const pathview_type & path) const;
		/// find element by given path starting from given root, if no such element is found - invalid index returned
		virtual QModelIndex find_element(const QModelIndex & root, const pathview_type & path) const;

	public:
		virtual int rowCount(const QModelIndex & parent = model_helper::invalid_index) const override;
		virtual QModelIndex parent(const QModelIndex & index) const override;
		virtual QModelIndex index(int row, int column, const QModelIndex & parent = model_helper::invalid_index) const override;

	protected:
		/// recalculates page on some changes, updates/inserts/erases,
		/// called after all children of page are already processed and recalculated
		virtual void recalculate_page(page_type & page) = 0;

	protected:
		/// emits qt signal this->dataChanged about changed rows. Changred rows are defined by [first; last)
		/// default implantation just calls this->dataChanged(index(row, 0, parent), index(row, this->columnCount, parent))
		virtual void emit_changed(QModelIndex parent, int_vector::const_iterator first, int_vector::const_iterator last);
		/// changes persistent indexes via this->changePersistentIndex.
		/// [first; last) - range where range[oldIdx - offset] => newIdx.
		/// if newIdx < 0 - index should be removed(changed on invalid, qt supports it)
		virtual void change_indexes(page_type & page, QModelIndexList::const_iterator model_index_first, QModelIndexList::const_iterator model_index_last,
		                            int_vector::const_iterator first, int_vector::const_iterator last, int offset);
		/// inverses index array in following way:
		/// inverse[arr[i] - offset] = i for first..last.
		/// This is for when you have array of arr[new_index] => old_index,
		/// but need arr[old_index] => new_idx for qt changePersistentIndex
		void inverse_index_array(int_vector & inverse, int_vector::iterator first, int_vector::iterator last, int offset);

	protected:
		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		virtual void merge_newdata(
			ivalue_ptr_iterator first, ivalue_ptr_iterator middle, ivalue_ptr_iterator last,
			bool resort_old = true);
		
		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		/// 
		/// range [ifirst, imiddle, ilast) must be permuted the same way as range [first, middle, last)
		virtual void merge_newdata(
			ivalue_ptr_iterator first, ivalue_ptr_iterator middle, ivalue_ptr_iterator last,
			int_vector::iterator ifirst, int_vector::iterator imiddle, int_vector::iterator ilast,
			bool resort_old = true);
		
		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		virtual void stable_sort(ivalue_ptr_iterator first, ivalue_ptr_iterator last);
		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		/// range [ifirst; ilast) must be permuted the same way as range [first; last)
		virtual void stable_sort(ivalue_ptr_iterator first, ivalue_ptr_iterator last,
		                         int_vector::iterator ifirst, int_vector::iterator ilast);

		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		/// emits qt layoutAboutToBeChanged(..., VerticalSortHint), layoutUpdated(..., VerticalSortHint)
		virtual void sort_and_notify();
		virtual void sort_and_notify(page_type & page, resort_context & ctx);

		/// refilters m_store with m_filter_pred according to rtype:
		/// * same        - does nothing and immediately returns(does not emit any qt signals)
		/// * incremental - calls refilter_full_and_notify
		/// * full        - calls refilter_incremental_and_notify
		virtual void refilter_and_notify(viewed::refilter_type rtype);
		/// removes elements not passing m_filter_pred from m_store
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_incremental_and_notify();
		virtual void refilter_incremental_and_notify(page_type & page, refilter_context & ctx);
		/// fills m_store from owner with values passing m_filter_pred and sorts them according to m_sort_pred
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_full_and_notify();
		virtual void refilter_full_and_notify(page_type & page, refilter_context & ctx);

	protected:
		/// helper method for copying update_context with newpath
		template <class update_context> static update_context copy_context(const update_context & ctx, pathview_type newpath);

		template <class update_context> auto process_erased(page_type & page, update_context & ctx)   -> std::tuple<pathview_type &, pathview_type &>;
		template <class update_context> auto process_updated(page_type & page, update_context & ctx)  -> std::tuple<pathview_type &, pathview_type &>;
		template <class update_context> auto process_inserted(page_type & page, update_context & ctx) -> std::tuple<pathview_type &, pathview_type &>;
		template <class update_context> void rearrange_children_and_notify(page_type & page, update_context & ctx);
		template <class update_context> void update_page_and_notify(page_type & page, update_context & ctx);
		
		template <class reset_context> void reset_page(page_type & page, reset_context & ctx);

	protected:
		/// updates internal element tree by processing given erased, updated and inserted leaf elements, those should be groped by segments
		/// nodes are crated as necessary leafs are inserted into nodes where needed, elements are rearranged according to current filter and sort criteria.
		template <class ErasedRandomAccessIterator, class UpdatedRandomAccessIterator, class InsertedRandomAccessIterator>
		void update_data_and_notify(
			ErasedRandomAccessIterator erased_first, ErasedRandomAccessIterator erased_last,
			UpdatedRandomAccessIterator updated_first, UpdatedRandomAccessIterator updated_last,
			InsertedRandomAccessIterator inserted_first, InsertedRandomAccessIterator inserted_last);

	public:
		const auto & sort_pred()   const { return m_sort_pred; }
		const auto & filter_pred() const { return m_filter_pred; }

		template <class ... Args> auto filter_by(Args && ... args) -> refilter_type;
		template <class ... Args> void sort_by(Args && ... args);

	public:
		sftree_facade_qtbase(QObject * parent = nullptr) : model_base(parent) {}
		sftree_facade_qtbase(traits_type traits, QObject * parent = nullptr) : model_base(parent), traits_type(std::move(traits)) {}
		virtual ~sftree_facade_qtbase() = default;
	};

	/************************************************************************/
	/*                   node_accessor_type definition                     */
	/************************************************************************/

	/// special predicate proxy that passes arguments as is, except for page references/pointers:
	/// those are translated to node: return page.node | return &page->node
	///
	/// consider you have same type leaf/node, or their members have same name:
	///  now you want to write something like: viewed::visit([](auto * item) { return item->something; }, ivalue_ptr var);
	///  sadly it will not work, because ivalue_ptr - variant of leaf and page, and page holds node as node member.
	///
	/// with this class you can write:
	///  viewed::visit(node_accessor([](auto * item) { item->something; }, ivalue_ptr var);
	template <class Traits, class ModelBase>
	struct sftree_facade_qtbase<Traits, ModelBase>::node_accessor_type
	{
		template <class Arg>
		static decltype(auto) unwrap(Arg && arg) noexcept { return std::forward<Arg>(arg); }

		static const node_type * unwrap(const page_type * page) noexcept { return &page->node; }
		static       node_type * unwrap(      page_type * page) noexcept { return &page->node; }
		static const node_type & unwrap(const page_type & page) noexcept { return page.node;   }
		static       node_type & unwrap(      page_type & page) noexcept { return page.node;   }

		// direct call form
		template <class Pred, class ... Args>
		auto operator()(Pred && pred, Args && ... args) const
		{
			return std::forward<Pred>(pred)(unwrap(std::forward<Args>(args))...);
		}

		// predicate wrapper form
		template <class Pred>
		auto operator()(Pred && pred) const
		{
			return [pred = std::forward<Pred>(pred)](auto && ... args) mutable -> decltype(auto)
			{
				return std::move(pred)(unwrap(std::forward<decltype(args)>(args))...);
			};
		}
	};

	/************************************************************************/
	/*                  value_ptr_filter_type definition                    */
	/************************************************************************/
	template <class Traits, class ModelBase>
	template <class Pred>
	struct sftree_facade_qtbase<Traits, ModelBase>::ivalue_ptr_filter_type
	{
		// if node and leaf are the same types - we should provide a hint to predicate what argument is: LEAF or PAGE
		// but only if Predicate accepts those hints
		static constexpr auto HintedCall = std::is_same_v<leaf_type, node_type> and std::is_invocable_v<Pred, const leaf_type &, std::integer_sequence<unsigned, LEAF>>;
		using pred_type = std::conditional_t<HintedCall, Pred, typename viewed::make_indirect_pred_type<Pred>::type>;

		pred_type pred;

		ivalue_ptr_filter_type(Pred pred) : pred(std::move(pred)) {}
		bool operator()(const ivalue_ptr & v) const;
	};

	template <class Traits, class ModelBase>
	template <class Pred>
	bool sftree_facade_qtbase<Traits, ModelBase>::ivalue_ptr_filter_type<Pred>::operator()(const ivalue_ptr & v) const
	{
		if constexpr(not HintedCall)
			return get_children_count(v) > 0 or viewed::visit(node_accessor(pred), v);
		else
		{
			// if node and leaf are the same types - we should provide a hint to predicate what argument is: LEAF or PAGE
			switch (v.index())
			{
				case 0: { auto * page = static_cast<page_type *>(v.pointer()); return page->nvisible > 0 or pred(page->node, std::integer_sequence<unsigned, PAGE>()); }
				case 1: return pred(*static_cast<leaf_type *>(v.pointer()), std::integer_sequence<unsigned, LEAF>());
				default: EXT_UNREACHABLE();
			}
		}
	}

	/************************************************************************/
	/*                  value_ptr_sorter_type definition                    */
	/************************************************************************/
	template <class Traits, class ModelBase>
	template <class Pred>
	struct sftree_facade_qtbase<Traits, ModelBase>::ivalue_ptr_sorter_type
	{
		// if node and leaf are the same types - we should provide a hint to predicate what arguments are: LEAF or PAGE
		// but only if Predicate accepts those hints
		static constexpr auto HintedCall = std::is_same_v<leaf_type, node_type> and std::is_invocable_v<Pred, const leaf_type &, const leaf_type &, std::integer_sequence<unsigned, LEAF, LEAF>>;
		using pred_type = std::conditional_t<HintedCall, Pred, typename viewed::make_indirect_pred_type<Pred>::type>;
		pred_type pred;

		ivalue_ptr_sorter_type(Pred pred) : pred(std::move(pred)) {}

		bool operator()(const ivalue_ptr & v1, const ivalue_ptr & v2) const;
	};

	template <class Traits, class ModelBase>
	template <class Pred>
	bool sftree_facade_qtbase<Traits, ModelBase>::ivalue_ptr_sorter_type<Pred>::operator()(const ivalue_ptr & v1, const ivalue_ptr & v2) const
	{
		if constexpr (not HintedCall)
			return viewed::visit(node_accessor(pred), v1, v2);
		else
		{
			// if node and leaf are the same types - we should provide a hint to predicate what arguments are: LEAF or PAGE
			auto type = v1.index() * 2 + v2.index();
			switch (type)
			{
				case 0: return node_accessor(pred, *static_cast<const page_type *>(v1.pointer()), *static_cast<const page_type *>(v2.pointer()), std::integer_sequence<unsigned, PAGE, PAGE>());
				case 1: return node_accessor(pred, *static_cast<const page_type *>(v1.pointer()), *static_cast<const leaf_type *>(v2.pointer()), std::integer_sequence<unsigned, PAGE, LEAF>());
				case 2: return node_accessor(pred, *static_cast<const leaf_type *>(v1.pointer()), *static_cast<const page_type *>(v2.pointer()), std::integer_sequence<unsigned, LEAF, PAGE>());
				case 3: return node_accessor(pred, *static_cast<const leaf_type *>(v1.pointer()), *static_cast<const leaf_type *>(v2.pointer()), std::integer_sequence<unsigned, LEAF, LEAF>());
				default: EXT_UNREACHABLE();
			}
		}
	}

	/************************************************************************/
	/*                  Methods implementations                             */
	/************************************************************************/
	template <class Traits, class ModelBase>
	const typename sftree_facade_qtbase<Traits, ModelBase>::ivalue_container sftree_facade_qtbase<Traits, ModelBase>::ms_empty_container;

	template <class Traits, class ModelBase>
	const typename sftree_facade_qtbase<Traits, ModelBase>::pathview_type sftree_facade_qtbase<Traits, ModelBase>::ms_empty_path;


	template <class Traits, class ModelBase>
	template <class Functor>
	void sftree_facade_qtbase<Traits, ModelBase>::for_each_child_page(page_type & page, Functor && func)
	{
		for (auto & child : page.children)
		{
			if (child.index() == PAGE)
			{
				auto * child_page = static_cast<page_type *>(child.pointer());
				std::forward<Functor>(func)(*child_page);
			}
		}
	}

	template <class Traits, class ModelBase>
	template <class RandomAccessIterator>
	void sftree_facade_qtbase<Traits, ModelBase>::group_by_paths(RandomAccessIterator first, RandomAccessIterator last)
	{
		std::sort(first, last, path_group_pred);
	}

	/************************************************************************/
	/*       QAbstractItemModel tree implementation                         */
	/************************************************************************/
	template <class Traits, class ModelBase>
	QModelIndex sftree_facade_qtbase<Traits, ModelBase>::create_index(int row, int column, page_type * ptr) const
	{
		return this->createIndex(row, column, ptr);
	}

	template <class Traits, class ModelBase>
	inline auto sftree_facade_qtbase<Traits, ModelBase>::get_page(const QModelIndex & index) const -> page_type *
	{
		assert(index.isValid());
		return static_cast<page_type *>(index.internalPointer());
	}

	template <class Traits, class ModelBase>
	auto sftree_facade_qtbase<Traits, ModelBase>::get_ielement_ptr(const QModelIndex & index) const -> const ivalue_ptr &
	{
		auto * page = get_page(index);
		assert(page);

		auto & seq_view = page->children.template get<by_seq>();
		assert(index.row() < get_children_count(page));
		return seq_view[index.row()];
	}

	template <class Traits, class ModelBase>
	int sftree_facade_qtbase<Traits, ModelBase>::rowCount(const QModelIndex & parent) const
	{
		if (not parent.isValid())
			return qint(get_children_count(&m_root));

		const auto & val = get_ielement_ptr(parent);
		return qint(get_children_count(val));
	}

	template <class Traits, class ModelBase>
	QModelIndex sftree_facade_qtbase<Traits, ModelBase>::parent(const QModelIndex & index) const
	{
		if (not index.isValid()) return {};

		page_type * page = get_page(index);
		assert(page);

		page_type * parent_page = page->parent;
		if (not parent_page) return {}; // already top level index

		auto & children = parent_page->children;
		auto & seq_view = children.template get<by_seq>();

		auto code_it = children.find(get_name(page));
		auto seq_it  = children.template project<by_seq>(code_it);

		int row = qint(seq_it - seq_view.begin());
		return create_index(row, 0, parent_page);
	}

	template <class Traits, class ModelBase>
	QModelIndex sftree_facade_qtbase<Traits, ModelBase>::index(int row, int column, const QModelIndex & parent) const
	{
		if (not parent.isValid())
		{
			return create_index(row, column, ext::unconst(&m_root));
		}
		else
		{
			auto & element = get_ielement_ptr(parent);
			auto & children = get_children(element);
			auto count = get_children_count(element);

			if (count < row)
				return {};

			// only page can have children
			assert(element.index() == PAGE);
			auto * page = static_cast<page_type *>(element.pointer());
			return create_index(row, column, page);
		}
	}

	template <class Traits, class ModelBase>
	inline QModelIndex sftree_facade_qtbase<Traits, ModelBase>::find_element(const pathview_type & path) const
	{
		return find_element(model_helper::invalid_index, path);
	}

	template <class Traits, class ModelBase>
	QModelIndex sftree_facade_qtbase<Traits, ModelBase>::find_element(const QModelIndex & root, const pathview_type & path) const
	{
		std::uintptr_t type;
		const page_type * cur_page;
		if (not root.isValid())
			cur_page = &m_root;
		else
		{
			const auto & val_ptr = get_ielement_ptr(root);
			if (val_ptr.index() == LEAF)
				return QModelIndex(); // leafs do not have children
			else
				cur_page = static_cast<const page_type *>(val_ptr.pointer());
		}

		pathview_type curpath = ms_empty_path;
		pathview_type name;

		for (;;)
		{
			std::tie(type, name, curpath) = parse_path(path, curpath);

			auto & children = cur_page->children;
			auto & seq_view = children.template get<by_seq>();

			auto code_it = children.find(name);
			if (code_it == children.end()) return QModelIndex();

			auto seq_it  = children.template project<by_seq>(code_it);
			int row = qint(seq_it - seq_view.begin());
			if (row >= cur_page->nvisible) return QModelIndex();

			if (type == LEAF)
				return create_index(row, 0, ext::unconst(cur_page));
			else
			{
				cur_page = static_cast<const page_type *>(code_it->pointer());
				continue;
			}
		}
	}

	/************************************************************************/
	/*                     qt emit helpers                                  */
	/************************************************************************/
	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::emit_changed(QModelIndex parent, int_vector::const_iterator first, int_vector::const_iterator last)
	{
		if (first == last) return;

		auto * that = static_cast<const QAbstractItemModel *>(this);
		int ncols = that->columnCount(parent);
		for (; first != last; ++first)
		{
			// lower index on top, higher on bottom
			int top, bottom;
			top = bottom = *first;

			// try to find the sequences with step of 1, for example: ..., 4, 5, 6, ...
			for (++first; first != last and *first - bottom == 1; ++first, ++bottom)
				continue;

			--first;

			auto top_left = this->index(top, 0, parent);
			auto bottom_right = this->index(bottom, ncols - 1, parent);
			this->dataChanged(top_left, bottom_right, model_helper::all_roles);
		}
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::change_indexes(page_type & page, QModelIndexList::const_iterator model_index_first, QModelIndexList::const_iterator model_index_last, int_vector::const_iterator first, int_vector::const_iterator last, int offset)
	{
		auto size = last - first; (void)size;

		for (; model_index_first != model_index_last; ++model_index_first)
		{
			const QModelIndex & idx = *model_index_first;
			if (not idx.isValid()) continue;

			auto * pageptr = get_page(idx);
			if (pageptr != &page) continue;

			auto row = idx.row();
			auto col = idx.column();

			if (row < offset) continue;

			assert(row < size); (void)size;
			auto newIdx = create_index(first[row - offset], col, pageptr);
			this->changePersistentIndex(idx, newIdx);
		}
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::inverse_index_array(int_vector & inverse, int_vector::iterator first, int_vector::iterator last, int offset)
	{
		inverse.resize(last - first);
		int i = offset;

		for (auto it = first; it != last; ++it, ++i)
		{
			int val = *it;
			inverse[viewed::unmark_index(val) - offset] = not viewed::marked_index(val) ? i : -1;
		}
	}

	/************************************************************************/
	/*                    sort/filter support                               */
	/************************************************************************/
	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::merge_newdata(
		ivalue_ptr_iterator first, ivalue_ptr_iterator middle, ivalue_ptr_iterator last, bool resort_old /* = true */)
	{
		if (not viewed::active(m_sort_pred)) return;

		auto sorter = ivalue_ptr_sorter_type(std::cref(m_sort_pred));
		auto comp = viewed::make_indirect_fun(std::move(sorter));

		if (resort_old) varalgo::stable_sort(first, middle, comp);
		varalgo::sort(middle, last, comp);
		varalgo::inplace_merge(first, middle, last, comp);

	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::merge_newdata(
		ivalue_ptr_iterator first, ivalue_ptr_iterator middle, ivalue_ptr_iterator last,
		int_vector::iterator ifirst, int_vector::iterator imiddle, int_vector::iterator ilast, bool resort_old /* = true */)
	{
		if (not viewed::active(m_sort_pred)) return;

		assert(last - first == ilast - ifirst);
		assert(middle - first == imiddle - ifirst);

		auto sorter = ivalue_ptr_sorter_type(std::cref(m_sort_pred));
		auto comp = viewed::make_get_functor<0>(viewed::make_indirect_fun(std::move(sorter)));

		auto zfirst  = ext::make_zip_iterator(first, ifirst);
		auto zmiddle = ext::make_zip_iterator(middle, imiddle);
		auto zlast   = ext::make_zip_iterator(last, ilast);

		if (resort_old) varalgo::stable_sort(zfirst, zmiddle, comp);
		varalgo::sort(zmiddle, zlast, comp);
		varalgo::inplace_merge(zfirst, zmiddle, zlast, comp);
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::stable_sort(ivalue_ptr_iterator first, ivalue_ptr_iterator last)
	{
		if (not viewed::active(m_sort_pred)) return;

		auto sorter = ivalue_ptr_sorter_type(std::cref(m_sort_pred));
		auto comp = viewed::make_indirect_fun(std::move(sorter));
		varalgo::stable_sort(first, last, comp);
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::stable_sort(
		ivalue_ptr_iterator first, ivalue_ptr_iterator last,
		int_vector::iterator ifirst, int_vector::iterator ilast)
	{
		if (not viewed::active(m_sort_pred)) return;

		auto sorter = ivalue_ptr_sorter_type(std::cref(m_sort_pred));
		auto comp = viewed::make_get_functor<0>(viewed::make_indirect_fun(std::move(sorter)));

		auto zfirst = ext::make_zip_iterator(first, ifirst);
		auto zlast = ext::make_zip_iterator(last, ilast);
		varalgo::stable_sort(zfirst, zlast, comp);
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::sort_and_notify()
	{
		if (not viewed::active(m_sort_pred)) return;

		resort_context ctx;
		int_vector index_array, inverse_buffer_array;
		ivalue_ptr_vector valptr_array;

		ctx.index_array = &index_array;
		ctx.inverse_array = &inverse_buffer_array;
		ctx.vptr_array = &valptr_array;

		this->layoutAboutToBeChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);

		auto indexes = this->persistentIndexList();
		ctx.model_index_first = indexes.begin();
		ctx.model_index_last = indexes.end();

		sort_and_notify(m_root, ctx);

		this->layoutChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::sort_and_notify(page_type & page, resort_context & ctx)
	{
		auto & container = page.children;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;
		constexpr int offset = 0;

		ivalue_ptr_vector & valptr_vector = *ctx.vptr_array;
		int_vector & index_array = *ctx.index_array;
		int_vector & inverse_array = *ctx.inverse_array;

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		index_array.resize(seq_ptr_view.size());

		auto first  = valptr_vector.begin();
		auto middle = first + page.nvisible;
		auto last   = valptr_vector.end();

		auto ifirst  = index_array.begin();
		auto imiddle = ifirst + page.nvisible;
		auto ilast   = index_array.end();

		std::iota(ifirst, ilast, offset);
		stable_sort(first, middle, ifirst, imiddle);

		seq_view.rearrange(boost::make_transform_iterator(first, make_ref));
		inverse_index_array(inverse_array, ifirst, ilast, offset);
		change_indexes(page, ctx.model_index_first, ctx.model_index_last,
					   inverse_array.begin(), inverse_array.end(), offset);

		for_each_child_page(page, [this, &ctx](auto & page) { sort_and_notify(page, ctx); });
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::refilter_and_notify(viewed::refilter_type rtype)
	{
		switch (rtype)
		{
			default:
			case viewed::refilter_type::same:        return;
			
			case viewed::refilter_type::incremental: return refilter_incremental_and_notify();
			case viewed::refilter_type::full:        return refilter_full_and_notify();
		}
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::refilter_incremental_and_notify()
	{
		refilter_context ctx;
		int_vector index_array, inverse_buffer_array;
		ivalue_ptr_vector valptr_array;

		ctx.index_array = &index_array;
		ctx.inverse_array = &inverse_buffer_array;
		ctx.vptr_array = &valptr_array;

		this->layoutAboutToBeChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);

		auto indexes = this->persistentIndexList();
		ctx.model_index_first = indexes.begin();
		ctx.model_index_last = indexes.end();

		refilter_incremental_and_notify(m_root, ctx);

		this->layoutChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::refilter_incremental_and_notify(page_type & page, refilter_context & ctx)
	{
		for_each_child_page(page, [this, &ctx](auto & page) { refilter_incremental_and_notify(page, ctx); });

		auto & container = page.children;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;
		constexpr int offset = 0;

		// implementation is similar to refilter_full_and_notify,
		// but more simple - only visible area should filtered, and no sorting should be done
		// refilter_full_and_notify - for more description

		ivalue_ptr_vector & valptr_vector = *ctx.vptr_array;
		int_vector & index_array = *ctx.index_array;
		int_vector & inverse_array = *ctx.inverse_array;

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		index_array.resize(seq_ptr_view.size());

		auto filter = ivalue_ptr_filter_type(std::cref(m_filter_pred));
		auto fpred  = viewed::make_indirect_fun(std::move(filter));
		auto zfpred = viewed::make_get_functor<0>(fpred);

		auto vfirst = valptr_vector.begin();
		auto vlast  = vfirst + page.nvisible;

		auto ivfirst = index_array.begin();
		auto ivlast  = ivfirst + page.nvisible;
		auto isfirst = ivlast;
		auto islast  = index_array.end();

		std::iota(ivfirst, islast, offset);

		auto[vpp, ivpp] = std::stable_partition(
			ext::make_zip_iterator(vfirst, ivfirst),
			ext::make_zip_iterator(vlast, ivlast),
			zfpred).get_iterator_tuple();

		std::transform(ivpp, ivlast, ivpp, viewed::mark_index);

		int nvisible_new = vpp - vfirst;
		seq_view.rearrange(boost::make_transform_iterator(vfirst, make_ref));
		page.nvisible = nvisible_new;

		inverse_index_array(inverse_array, index_array.begin(), index_array.end(), offset);
		change_indexes(page, ctx.model_index_first, ctx.model_index_last,
					   inverse_array.begin(), inverse_array.end(), offset);
	}

	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::refilter_full_and_notify()
	{
		refilter_context ctx;
		int_vector index_array, inverse_buffer_array;
		ivalue_ptr_vector valptr_array;

		ctx.index_array = &index_array;
		ctx.inverse_array = &inverse_buffer_array;
		ctx.vptr_array = &valptr_array;

		this->layoutAboutToBeChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);

		auto indexes = this->persistentIndexList();
		ctx.model_index_first = indexes.begin();
		ctx.model_index_last = indexes.end();

		refilter_full_and_notify(m_root, ctx);

		this->layoutChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);
	}
	
	template <class Traits, class ModelBase>
	void sftree_facade_qtbase<Traits, ModelBase>::refilter_full_and_notify(page_type & page, refilter_context & ctx)
	{
		for_each_child_page(page, [this, &ctx](auto & page) { refilter_full_and_notify(page, ctx); });

		auto & container = page.children;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;
		constexpr int offset = 0;
		int nvisible_new;

		ivalue_ptr_vector & valptr_vector = *ctx.vptr_array;
		int_vector & index_array = *ctx.index_array;
		int_vector & inverse_array = *ctx.inverse_array;

		// We must rearrange children according to sorting/filtering criteria.
		// Note when working with visible area - order of visible elements should not changed - we want stability.
		// Also Qt persistent indexes should be recalculated.
		//
		// Because children are stored in boost::multi_index_container we must copy pointer to elements into separate vector,
		// rearrange them, and than call boost::multi_index_container::rearrange
		//
		// layout of elements at start:
		//
		// |vfirst                     |vlast                      |valptr_vector.end()
		// ---------------------------------------------------------
		// |    visible elements       |      shadow elements      |
		// ---------------------------------------------------------
		// |valptr_vector.begin()      |sfirst                     |slast
		//

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		index_array.resize(seq_ptr_view.size());

		auto filter = ivalue_ptr_filter_type(std::cref(m_filter_pred));
		auto fpred  = viewed::make_indirect_fun(std::move(filter));
		auto zfpred = viewed::make_get_functor<0>(fpred);

		auto vfirst = valptr_vector.begin();
		auto vlast  = vfirst + page.nvisible;
		auto sfirst = vlast;
		auto slast  = valptr_vector.end();

		// elements index array - it will be permutated with elements array, later it will be used for recalculating qt indexes
		auto ivfirst = index_array.begin();
		auto ivlast  = ivfirst + page.nvisible;
		auto isfirst = ivlast;
		auto islast  = index_array.end();
		std::iota(ivfirst, islast, offset);

		if (not viewed::active(m_filter_pred))
		{
			// if there is now filter - just merge shadow area with visible
			nvisible_new = slast - vfirst;
			merge_newdata(vfirst, vlast, slast, ivfirst, ivlast, islast, false);
		}
		else
		{
			// partition visible area against filter predicate
			auto [vpp, ivpp] = std::stable_partition(
				ext::make_zip_iterator(vfirst, ivfirst),
				ext::make_zip_iterator(vlast, ivlast),
				zfpred).get_iterator_tuple();

			// partition shadow area against filter predicate
			auto [spp, ispp] = std::partition(
				ext::make_zip_iterator(sfirst, isfirst),
				ext::make_zip_iterator(slast, islast),
				zfpred).get_iterator_tuple();

			// mark indexes if elements that do not pass filtering as removed, to outside world they are removed
			std::transform(ivpp, ivlast, ivpp, viewed::mark_index);
			std::transform(ispp, islast, ispp, viewed::mark_index);

			// current layout of elements:
			// P - passes filter, X - not passes filter
			// |vfirst                 |vlast
			// -------------------------------------------------
			// |P|P|P|P|P|P|X|X|X|X|X|X|P|P|P|P|P|X|X|X|X|X|X|X|
			// -------------------------------------------------
			// |           |           |sfirst   |             |slast
			//             |vpp                  |spp

			// rotate [sfirst; spp) at the the at of visible area, that passes filter
			vlast  = std::rotate(vpp, sfirst, spp);
			ivlast = std::rotate(ivpp, isfirst, ispp);
			nvisible_new = vlast - vfirst;
			// merge new elements from shadow area
			merge_newdata(vfirst, vpp, vlast, ivfirst, ivpp, ivlast, false);
		}

		// rearranging is over -> set order in boost::multi_index_container
		seq_view.rearrange(boost::make_transform_iterator(vfirst, make_ref));
		page.nvisible = nvisible_new;

		// recalculate qt persistent indexes and notify any clients
		inverse_index_array(inverse_array, index_array.begin(), index_array.end(), offset);
		change_indexes(page, ctx.model_index_first, ctx.model_index_last,
					   inverse_array.begin(), inverse_array.end(), offset);
	}

	template <class Traits, class ModelBase>
	template <class ... Args>
	auto sftree_facade_qtbase<Traits, ModelBase>::filter_by(Args && ... args) -> viewed::refilter_type
	{
		refilter_type rtype = m_filter_pred.set_expr(std::forward<Args>(args)...);
		refilter_and_notify(rtype);

		return rtype;
	}

	template <class Traits, class ModelBase>
	template <class ... Args>
	void sftree_facade_qtbase<Traits, ModelBase>::sort_by(Args && ... args)
	{
		m_sort_pred = sort_pred_type(std::forward<Args>(args)...);
		sort_and_notify();
	}

	/************************************************************************/
	/*               reset_data methods implementation                      */
	/************************************************************************/	 
	template <class Traits, class ModelBase>
	template <class reset_context>
	void sftree_facade_qtbase<Traits, ModelBase>::reset_page(page_type & page, reset_context & ctx)
	{
		auto & container = page.children;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		while (ctx.first != ctx.last)
		{
			// with current path parse each element:
			auto && item_ptr = *ctx.first;
			auto [type, name, newpath] = parse_path(this->get_path(*item_ptr), ctx.path);
			if (type == LEAF)
			{
				// if it's leaf: add to children
				container.insert(std::forward<decltype(item_ptr)>(item_ptr));
				++ctx.first;
			}
			else // PAGE
			{
				// it's page: prepare new context - for extracted page
				auto newctx = ctx;
				newctx.path = std::move(newpath);
				newctx.first = ctx.first;

				// extract node sub-range
				auto is_child = [this, &path = ctx.path, &name](const auto * item) { return this->is_child(path, name, this->get_path(*item)); };
				newctx.last = std::find_if_not(ctx.first, ctx.last, is_child);
				// create new page
				auto page_ptr = std::make_unique<page_type>();
				page_ptr->parent = &page;
				traits_type::set_name(page_ptr->node, std::move(ctx.path), std::move(name));
				// process child node recursively
				reset_page(*page_ptr, newctx);

				container.insert(std::move(page_ptr));
				ctx.first = newctx.last;
			}
		}

		// rearrange children according to filtering/sorting criteria
		ivalue_ptr_vector & refs = *ctx.vptr_array;
		refs.assign(seq_ptr_view.begin(), seq_ptr_view.end());

		auto filter = ivalue_ptr_filter_type(std::cref(m_filter_pred));
		auto fpred  = viewed::make_indirect_fun(std::move(filter));

		auto refs_first = refs.begin();
		auto refs_last  = refs.end();

		// apply filtering
		auto refs_pp = viewed::active(fpred)
		    ? std::partition(refs_first, refs_last, fpred)
		    : refs_last;

		// apply sorting
		stable_sort(refs_first, refs_pp);

		page.nvisible = refs_pp - refs_first;
		seq_view.rearrange(boost::make_transform_iterator(refs_first, make_ref));

		// and recalculate page
		this->recalculate_page(page);
	}

	/************************************************************************/
	/*               update_data methods implementation                     */
	/************************************************************************/
	template <class Traits, class ModelBase>
	template <class update_context>
	auto sftree_facade_qtbase<Traits, ModelBase>::copy_context(const update_context & ctx, pathview_type newpath) -> update_context
	{
		update_context newctx;
		newctx.inserted_first = ctx.inserted_first;
		newctx.inserted_last = ctx.inserted_last;
		newctx.updated_first = ctx.updated_first;
		newctx.updated_last = ctx.updated_last;
		newctx.erased_first = ctx.erased_first;
		newctx.erased_last = ctx.erased_last;

		newctx.changed_first = newctx.changed_last = ctx.changed_first;
		newctx.removed_last = newctx.removed_first = ctx.removed_last;

		newctx.path = std::move(newpath);

		newctx.vptr_array = ctx.vptr_array;
		newctx.index_array = ctx.index_array;
		newctx.inverse_array = ctx.inverse_array;

		newctx.model_index_first = ctx.model_index_first;
		newctx.model_index_last = ctx.model_index_last;

		return newctx;
	}

	template <class Traits, class ModelBase>
	template <class update_context>
	auto sftree_facade_qtbase<Traits, ModelBase>::process_erased(page_type & page, update_context & ctx) -> std::tuple<pathview_type &, pathview_type &>
	{
		auto & container = page.children;
		auto & seq_view  = container.template get<by_seq>();
		auto & code_view = container.template get<by_code>();

		// consumed nothing from previous step, return same name/path
		if (ctx.erased_diff == 0)
			return std::tie(ctx.erased_name, ctx.erased_path);

		for (; ctx.erased_first != ctx.erased_last; ++ctx.erased_first)
		{
			auto && item = *ctx.erased_first;
			std::uintptr_t type;
			std::tie(type, ctx.erased_name, ctx.erased_path) = parse_path(this->get_path(*item), ctx.path);
			if (type == PAGE) return std::tie(ctx.erased_name, ctx.erased_path);

			auto it = container.find(ctx.erased_name);
			assert(it != container.end());
			
			auto seqit = container.template project<by_seq>(it);
			auto pos = seqit - seq_view.begin();
			*ctx.removed_last++ = pos;

			// erasion will be done later, in rearrange_children_and_notify
			// container.erase(it);
		}

		ctx.erased_name = pathview_type();
		ctx.erased_path = pathview_type();
		return std::tie(ctx.erased_name, ctx.erased_path);
	}

	template <class Traits, class ModelBase>
	template <class update_context>
	auto sftree_facade_qtbase<Traits, ModelBase>::process_updated(page_type & page, update_context & ctx) -> std::tuple<pathview_type &, pathview_type &>
	{
		auto & container = page.children;
		auto & seq_view  = container.template get<by_seq>();
		auto & code_view = container.template get<by_code>();

		// consumed nothing from previous step, return same name/path
		if (ctx.updated_diff == 0)
			return std::tie(ctx.updated_name, ctx.updated_path);

		for (; ctx.updated_first != ctx.updated_last; ++ctx.updated_first)
		{
			auto && item = *ctx.updated_first;
			std::uintptr_t type;
			std::tie(type, ctx.updated_name, ctx.updated_path) = parse_path(this->get_path(*item), ctx.path);
			if (type == PAGE) return std::tie(ctx.updated_name, ctx.updated_path);

			auto it = container.find(ctx.updated_name);
			assert(it != container.end());

			auto seqit = container.template project<by_seq>(it);
			auto pos = seqit - seq_view.begin();
			*--ctx.changed_first = pos;
			
			ivalue_ptr & val = ext::unconst(*it);
			val = std::forward<decltype(item)>(item);
		}

		ctx.updated_name = pathview_type();
		ctx.updated_path = pathview_type();
		return std::tie(ctx.updated_name, ctx.updated_path);
	}

	template <class Traits, class ModelBase>
	template <class update_context>
	auto sftree_facade_qtbase<Traits, ModelBase>::process_inserted(page_type & page, update_context & ctx) -> std::tuple<pathview_type &, pathview_type &>
	{
		auto & container = page.children;
		auto & seq_view  = container.template get<by_seq>();
		auto & code_view = container.template get<by_code>();
		EXT_UNUSED(seq_view, code_view);
		
		// consumed nothing from previous step, return same name/path
		if (ctx.inserted_diff == 0)
			return std::tie(ctx.inserted_name, ctx.inserted_path);

		for (; ctx.inserted_first != ctx.inserted_last; ++ctx.inserted_first)
		{
			auto && item = *ctx.inserted_first;
			std::uintptr_t type;
			std::tie(type, ctx.inserted_name, ctx.inserted_path) = parse_path(this->get_path(*item), ctx.path);
			if (type == PAGE) return std::tie(ctx.inserted_name, ctx.inserted_path);

			decltype(container.begin()) it; bool inserted;
			std::tie(it, inserted) = container.insert(std::forward<decltype(item)>(item));
			assert(inserted or it->index() == PAGE); EXT_UNUSED(it, inserted);
		}

		ctx.inserted_name = pathview_type();
		ctx.inserted_path = pathview_type();
		return std::tie(ctx.inserted_name, ctx.inserted_path);
	}

	template <class Traits, class ModelBase>
	template <class update_context>
	void sftree_facade_qtbase<Traits, ModelBase>::update_page_and_notify(page_type & page, update_context & ctx)
	{
		const auto & path = ctx.path;
		auto & container = page.children;
		auto & seq_view  = container.template get<by_seq>();
		auto & code_view = container.template get<by_code>();
		auto oldsz = container.size();
		ctx.inserted_diff = ctx.updated_diff = ctx.erased_diff = -1;

		// we have 3 groups of elements: inserted, updated, erased
		// traits provide us with parse_path, is_child methods, with those we can break leafs elements into tree structure.

		for (;;)
		{
			// step 1: with current path parse_path from each element from groups:
			// * if it's leaf - we process it: add to children, remember it's index into updated/removed ones. removal is done later
			// * if it's page - we break out.
			// those method update context with their process: inserted_first, updated_first, erased_first, etc
			process_inserted(page, ctx);
			process_updated(page, ctx);
			process_erased(page, ctx);

			// step 2: At this point only pages are at front of ranges
			//  find max according to their path name, extract 3 sub-ranges according to that name from all 3 groups, and recursively process them
			auto newpath = std::max({ctx.erased_path, ctx.updated_path, ctx.inserted_path}, path_less);
			auto name = std::max({ctx.erased_name, ctx.updated_name, ctx.inserted_name}, path_less);
			// if name is empty - we actually processed all elements
			if (path_equal_to(name, ms_empty_path)) break;
			// prepare new context - for extracted page
			auto newctx = copy_context(ctx, std::move(newpath));
			// extract sub-ranges, also update iterators in current context, those elements will be processed in recursive call
			auto is_child = [this, &path, &name](auto && item) { return this->is_child(this->get_path(*item), path, name); };
			ctx.inserted_first = std::find_if_not(ctx.inserted_first, ctx.inserted_last, is_child);
			ctx.updated_first  = std::find_if_not(ctx.updated_first,  ctx.updated_last,  is_child);
			ctx.erased_first   = std::find_if_not(ctx.erased_first,   ctx.erased_last,   is_child);

			newctx.inserted_last = ctx.inserted_first;
			newctx.updated_last  = ctx.updated_first;
			newctx.erased_last   = ctx.erased_first;

			// how many elements in sub-groups
			ctx.inserted_diff = newctx.inserted_last - newctx.inserted_first;
			ctx.updated_diff  = newctx.updated_last  - newctx.updated_first;
			ctx.erased_diff   = newctx.erased_last   - newctx.erased_first;
			// if this assert fired, good chances that parse_path and is_child from traits are broken
			assert(ctx.inserted_diff or ctx.updated_diff or ctx.erased_diff);

			// find or create new page
			page_type * child_page = nullptr;
			bool inserted = false;
			auto it = container.find(name);
			if (it != container.end())
			{
				if (it->index() == PAGE)
					child_page = static_cast<page_type *>(it->pointer());
				else
				{
					// Element was a leaf, but we want a page now, replace it with new page.
					// This can happen on upsert operation with something like: "folder" -> "folder/file"
					assert(ctx.updated_diff or ctx.inserted_diff);
					auto child = std::make_unique<page_type>();
					child_page = child.get();

					child_page->parent = &page;
					traits_type::set_name(child_page->node, std::move(path), std::move(name));
					container.replace(it, std::move(child));
				}
			}
			else 
			{
				// if creating new page - there definitely was inserted or updated element
				assert(ctx.updated_diff or ctx.inserted_diff);
				auto child = std::make_unique<page_type>();
				child_page = child.get();

				child_page->parent = &page;
				traits_type::set_name(child_page->node, std::move(path), std::move(name));
				std::tie(it, inserted) = container.insert(std::move(child));
			}			

			// step 3: process child recursively
			update_page_and_notify(*child_page, newctx);

			// step 4: the child page itself is our child and as leafs is inserted/updated
			auto seqit = container.template project<by_seq>(it);
			auto pos = seqit - seq_view.begin();
			// if page does not have any children - it should be removed
			if (child_page->children.size() == 0)
				// actual erasion will be done later in rearrange_children_and_notify
				*ctx.removed_last++ = pos;
			else if (not inserted)
				// if it's updated - remember it's position as with leafs
				*--ctx.changed_first = pos;
		}

		// step 5: rearrange children according to filtering/sorting criteria
		ctx.inserted_count = container.size() - oldsz;
		ctx.updated_count  = ctx.changed_last - ctx.changed_first;
		ctx.erased_count   = ctx.removed_last - ctx.removed_first;

		rearrange_children_and_notify(page, ctx);
		// step 6: recalculate node from it's children
		this->recalculate_page(page);
	}

	template <class Traits, class ModelBase>
	template <class update_context>
	void sftree_facade_qtbase<Traits, ModelBase>::rearrange_children_and_notify(page_type & page, update_context & ctx)
	{
		auto & container = page.children;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;
		constexpr int offset = 0;
		int nvisible_new;

		ivalue_ptr_vector & valptr_vector = *ctx.vptr_array;
		int_vector & index_array = *ctx.index_array;
		int_vector & inverse_array = *ctx.inverse_array;

		auto filter = ivalue_ptr_filter_type(std::cref(m_filter_pred));
		auto fpred  = viewed::make_indirect_fun(std::move(filter));

		// We must rearrange children according to sorting/filtering criteria.
		// * some elements have been erased - those must be erased
		// * some elements have been inserted - those should placed in visible or shadow area if they do not pass filtering
		// * some elements have changed - those can move from/to visible/shadow area
		// * when working with visible area - order of visible elements should not changed - we want stability
		// And then visible elements should be resorted according to sorting criteria.
		// And Qt persistent indexes should be recalculated.
		// Removal of elements from boost::multi_index_container is O(n) operation, where n is distance from position to end of sequence,
		// so we better move those at back, and than remove them all at once - that way removal will be O(1)
		//
		// Because children are stored in boost::multi_index_container we must copy pointer to elements into separate vector,
		// rearrange them, and than call boost::multi_index_container::rearrange
		//
		// layout of elements at start:
		//
		// |vfirst                     |vlast                      |nfirst              |nlast
		// ------------------------------------------------------------------------------
		// |    visible elements       |      shadow elements      |    new elements    |
		// ------------------------------------------------------------------------------
		// |valptr_vector.begin()      |sfirst                     |slast               |valptr_vector.end()
		//

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		auto vfirst = valptr_vector.begin();
		auto vlast  = vfirst + page.nvisible;
		auto sfirst = vlast;
		auto slast  = vfirst + (seq_ptr_view.size() - ctx.inserted_count);
		auto nfirst = slast;
		auto nlast  = valptr_vector.end();

		// elements index array - it will be permutated with elements array, later it will be used for recalculating qt indexes
		index_array.resize(container.size());
		auto ifirst  = index_array.begin();
		auto imiddle = ifirst + page.nvisible;
		auto ilast   = index_array.end();
		std::iota(ifirst, ilast, offset); // at start: elements are placed in natural order: [0, 1, 2, ...]


		// [ctx.changed_first; ctx.changed_last) - indexes of changed elements
		// indexes < nvisible - are visible elements, others - are shadow.
		// partition that range by visible/shadow elements
		auto vchanged_first = ctx.changed_first;
		auto vchanged_last = std::partition(ctx.changed_first, ctx.changed_last,
			[nvisible = page.nvisible](int index) { return index < nvisible; });

		// now [vchanged_first; vchanged_last) - indexes of changed visible elements
		//     [schanged_first; schanged_last) - indexes of changed shadow elements
		auto schanged_first = vchanged_last;
		auto schanged_last = ctx.changed_last;

		// partition visible indexes by those passing filtering predicate
		auto index_pass_pred = [vfirst, fpred](int index) { return fpred(vfirst[index]); };
		auto vchanged_pp = viewed::active(m_filter_pred) 
			? std::partition(vchanged_first, vchanged_last, index_pass_pred)
			: vchanged_last;

		// now [vchanged_first; vchanged_pp) - indexes of visible elements passing filtering predicate
		//     [vchanged_pp; vchanged_last)  - indexes of visible elements not passing filtering predicate

		// mark removed ones by nullifying them, this will affect both shadow and visible elements
		// while we sort of forgetting about them, in fact we can always easily take them from seq_ptr_view, we still have their indexes
		for (auto it = ctx.removed_first; it != ctx.removed_last; ++it)
		{
			int index = *it;
			vfirst[index] = nullptr;
			ifirst[index] = -1;
		}

		// mark updated ones not passing filter predicate [vchanged_pp; vchanged_last) by nullifying them,
		// they have to be moved to shadow area, we will restore them little bit later
		for (auto it = vchanged_pp; it != vchanged_last; ++it)
		{
			int index = *it;
			vfirst[index] = nullptr;
			ifirst[index] = -1;
		}

		// current layout of elements:
		//  X - nullified
		//
		// |vfirst                     |vlast                      |nfirst               nlast
		// -------------------------------------------------------------------------------
		// | | | | | |X| | |X| | | | | | | | | | |X| | |X| | | | | | | | | | | | | | | | |
		// -------------------------------------------------------------------------------
		// |                           |sfirst                     |slast

		if (not viewed::active(m_filter_pred))
		{
			// remove marked elements from [vfirst; vlast)
			vlast  = std::remove(vfirst, vlast, nullptr);
			// remove marked elements from [sfirst; slast) but in reverse direction, gathering them near new elements
			sfirst = std::remove(std::make_reverse_iterator(slast), std::make_reverse_iterator(sfirst), nullptr).base();
			// and now just move gathered next to [vfirst; vlast)
			nlast  = std::move(sfirst, nlast, vlast);
			nvisible_new = nlast - vfirst;
		}
		else
		{
			// mark elements from shadow area passing filter predicate, by toggling lowest pointer bit
			for (auto it = schanged_first; it != schanged_last; ++it)
				if (index_pass_pred(*it)) vfirst[*it] = viewed::mark_pointer(vfirst[*it]);

			// current layout of elements:
			//  X - nullified, M - marked via lowest pointer bit
			//
			// |vfirst                     |vlast                      |nfirst               nlast
			// -------------------------------------------------------------------------------
			// | | | | | |X| | |X| | | | | | | | |M|M|X| | |X| | |M| | | | | | | | | | | | | |
			// -------------------------------------------------------------------------------
			// |                           |sfirst                     |slast

			// now remove X elements from [vfirst; vlast) - those are removed elements and should be completely removed
			vlast  = std::remove(vfirst, vlast, nullptr);
			// and remove X elements from [sfirst; slast) but in reverse direction, gathering them near new elements
			sfirst = std::remove(std::make_reverse_iterator(slast), std::make_reverse_iterator(sfirst), nullptr).base();

			// current layout of elements:
			//  X - nullified, M - marked via lowest pointer bit
			//
			//                             vlast <- 2 elements
			// |vfirst                 |vlast                          |nfirst               nlast
			// -------------------------------------------------------------------------------
			// | | | | | | | | | | | | | | | | | | | |M|M| | | | |M| | | | | | | | | | | | | |
			// -------------------------------------------------------------------------------
			// |                               |sfirst                 |slast
			//                             sfirst -> 2 elements

			// [spp, npp) - gathered elements from [sfirst, nlast) satisfying fpred
			auto spp = std::partition(sfirst, slast, [](auto * ptr) { return not viewed::marked_pointer(ptr); });
			auto npp = std::partition(nfirst, nlast, fpred);
			nvisible_new = (vlast - vfirst) + (npp - spp);

			// current layout of elements:
			//  X - nullified, M - marked via lowest pointer bit, S - not marked via lowest bit(not passing filter predicate)
			//  P - new elements passing filter predicate, N - new elements not passing filter predicate
			//
			// |vfirst                 |vlast                          |nfirst               nlast
			// -------------------------------------------------------------------------------
			// | | | | | | | | | | | | | | | | |S|S|S|S|S|S|S|S|S|M|M|M|P|P|P|P|P|P|N|N|N|N|N|
			// -------------------------------------------------------------------------------
			// |                               |sfirst           |     |slast      |
			//                                                   |spp              |npp

			// unmark any marked pointers
			for (auto it = spp; it != slast; ++it)
				*it = unmark_pointer(*it);

			// rotate [spp, npp) at the beginning of shadow area
			// and in fact merge those with visible area
			std::rotate(sfirst, spp, npp);
			nlast = std::move(sfirst, nlast, vlast);
		}

		// at that point we removed erased elements, but we need to rearrange boost::multi_index_container via rearrange method
		// and it expects all elements that it has - we need to add removed ones at the end of rearranged array - and them we will remove them from container.
		// [rfirst; rlast) - elements to be removed.
		auto rfirst = nlast;
		auto rlast = rfirst;

		{   // remove nullified elements from index array
			auto point = imiddle;
			imiddle = std::remove(ifirst, imiddle, -1);
			ilast = std::remove(point, ilast, -1);			
			ilast = std::move(point, ilast, imiddle);
		}
		
		// restore elements from [vchanged_pp; vchanged_last), add them at the end of shadow area
		for (auto it = vchanged_pp; it != vchanged_last; ++it)
		{
			int index = *it;
			*rlast++ = seq_ptr_view[index];
			*ilast++ = viewed::mark_index(index);
		}

		// temporary restore elements from [removed_first; removed_last), after container rearrange - they will be removed
		for (auto it = ctx.removed_first; it != ctx.removed_last; ++it)
		{
			int index = *it;
			*rlast++ = seq_ptr_view[index];
			*ilast++ = viewed::mark_index(index);
		}

		// if some elements from visible area are changed and they still in the visible area - we need to resort them => resort whole visible area.
		bool resort_old = vchanged_first != vchanged_pp;
		// resort visible area, merge new elements and changed from shadow area(std::stable sort + std::inplace_merge)
		merge_newdata(vfirst, vlast, nlast, ifirst, imiddle, ifirst + (nlast - vfirst), resort_old);
		// at last, rearranging is over -> set order it boost::multi_index_container
		seq_view.rearrange(boost::make_transform_iterator(vfirst, make_ref));
		// and erase removed elements
		seq_view.resize(seq_view.size() - ctx.erased_count);
		page.nvisible = nvisible_new;
		
		// recalculate qt persistent indexes and notify any clients
		inverse_index_array(inverse_array, ifirst, ilast, offset);
		change_indexes(page, ctx.model_index_first, ctx.model_index_last,
		               inverse_array.begin(), inverse_array.end(), offset);
	}

	template <class Traits, class ModelBase>
	template <class ErasedRandomAccessIterator, class UpdatedRandomAccessIterator, class InsertedRandomAccessIterator>
	void sftree_facade_qtbase<Traits, ModelBase>::update_data_and_notify(
		ErasedRandomAccessIterator erased_first, ErasedRandomAccessIterator erased_last,
		UpdatedRandomAccessIterator updated_first, UpdatedRandomAccessIterator updated_last,
		InsertedRandomAccessIterator inserted_first, InsertedRandomAccessIterator inserted_last)
	{
		using update_context = update_context_template<ErasedRandomAccessIterator, UpdatedRandomAccessIterator, InsertedRandomAccessIterator>;
		int_vector affected_indexes, index_array, inverse_buffer_array;
		ivalue_ptr_vector valptr_array;
		
		auto expected_indexes = erased_last - erased_first + std::max(updated_last - updated_first, inserted_last - inserted_first);
		affected_indexes.resize(expected_indexes);
		
		update_context ctx;
		ctx.index_array   = &index_array;
		ctx.inverse_array = &inverse_buffer_array;
		ctx.vptr_array    = &valptr_array;

		ctx.erased_first   = erased_first;
		ctx.erased_last    = erased_last;
		ctx.updated_first  = updated_first;
		ctx.updated_last   = updated_last;
		ctx.inserted_first = inserted_first;
		ctx.inserted_last  = inserted_last;

		ctx.removed_first = ctx.removed_last = affected_indexes.begin();
		ctx.changed_first = ctx.changed_last = affected_indexes.end();

		assert(std::is_sorted(ctx.erased_first, ctx.erased_last, path_group_pred));
		assert(std::is_sorted(ctx.updated_first, ctx.updated_last, path_group_pred));
		assert(std::is_sorted(ctx.inserted_first, ctx.inserted_last, path_group_pred));
		
		this->layoutAboutToBeChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);
		
		auto indexes = this->persistentIndexList();
		ctx.model_index_first = indexes.begin();
		ctx.model_index_last  = indexes.end();

		this->update_page_and_notify(m_root, ctx);

		this->layoutChanged(model_helper::empty_model_list, model_helper::NoLayoutChangeHint);
	}
}

#if BOOST_COMP_GNUC
#pragma GCC diagnostic pop
#endif
