//#include <boost/range.hpp>
//#include <boost/range/join.hpp>
//#include <boost/range/algorithm.hpp>
//#include <boost/range/algorithm_ext.hpp>
//#include <boost/range/adaptors.hpp>

#define BOOST_TEST_MODULE "viewed tests"
#include <boost/test/unit_test.hpp>

#include <viewed/hash_container_base.hpp>
#include <viewed/ordered_container_base.hpp>

#include <viewed/sfview_qtbase.hpp>
#include <viewed/selectable_sfview_qtbase.hpp>

template <class view_type>
class simple_qtmodel :
	public QAbstractListModel,
	public view_type
	//public viewed::view_qtbase<viewed::hash_container_base<int>>
{
	using base_type = view_type;
	using container_type = typename view_type::container_type;
	//using base_type = viewed::view_qtbase<viewed::hash_container_base<int>>;

public:
	QVariant QAbstractItemModel::data(const QModelIndex & idx, int role) const { return QVariant::fromValue(*m_store[idx.row()]); }
	int rowCount(const QModelIndex & parent = QModelIndex()) const override { return static_cast<int>(this->size()); }

public:
	simple_qtmodel(container_type * cont)
		: base_type(cont) { }
};

struct no_filter
{
	bool operator()(int i) const noexcept { return true; }
	operator bool() const noexcept { return false; }
};

struct odd_filter
{
	bool operator()(int i) const noexcept { return i % 2; }
	operator bool() const noexcept { return true; }
};


template <class Range1, class Range2>
static bool is_equal(const Range1 & r1, const Range2 & r2)
{
	std::vector<int> first, second;
	boost::push_back(first, r1);
	boost::push_back(second, r2);

	boost::sort(first);
	boost::sort(second);

	return boost::equal(first, second);
}

// test assign update erase
template <class container_type, class view_type>
static void test_aue()
{
	container_type cont;
	view_type view = &cont;
	view.init();

	std::vector<int> assign_batch1 = {10, 15, 1, 25, 100};
	std::vector<int> upsert_batch  = {1, -100};
	std::vector<int> assign_batch2 = {100, 25, 200, -100};

	cont.assign(assign_batch1.begin(), assign_batch1.end());
	BOOST_CHECK(is_equal(cont, assign_batch1));
	BOOST_CHECK(is_equal(view, assign_batch1));
	// implies is_equal(cont, view)

	cont.upsert(upsert_batch.begin(), upsert_batch.end());
	BOOST_CHECK(is_equal(cont, view));

	cont.assign(assign_batch2.begin(), assign_batch2.end());
	BOOST_CHECK(is_equal(cont, assign_batch2));
	BOOST_CHECK(is_equal(view, assign_batch2));

	cont.erase(100);
	cont.erase(-100);
	auto exprected = {25, 200};

	BOOST_CHECK(is_equal(cont, exprected));
	BOOST_CHECK(is_equal(view, exprected));
}


template <class View, class Range2>
static bool is_equal_sof(const View & view, const Range2 & rng)
{
	auto filter = view.filter_pred();
	auto sorter = view.sort_pred();

	std::vector<int> sof_rng;
	boost::push_back(sof_rng, rng);

	boost::remove_erase_if(sof_rng, [filter](auto & i) { return not filter(i); });
	boost::sort(sof_rng, sorter);

	return boost::equal(view, sof_rng);
}

// test assign update erase on sorted odd filtered view
template <class container_type, class view_type>
static void test_aue_sof()
{
	container_type cont;
	view_type view {&cont};
	view.init();

	std::vector<int> assign_batch1 = {10, 15, 1, 25, 100};
	std::vector<int> upsert_batch = {1, -100};
	std::vector<int> assign_batch2 = {100, 25, 200, -100, 7};

	cont.assign(assign_batch1.begin(), assign_batch1.end());
	BOOST_CHECK(is_equal_sof(view, cont));
	BOOST_CHECK(is_equal_sof(view, assign_batch1));
	// implies is_equal(cont, view)

	cont.upsert(upsert_batch.begin(), upsert_batch.end());
	BOOST_CHECK(is_equal_sof(view, cont));

	cont.assign(assign_batch2.begin(), assign_batch2.end());
	BOOST_CHECK(is_equal_sof(view, cont));
	BOOST_CHECK(is_equal_sof(view, assign_batch2));

	cont.erase(200);
	cont.erase(25);
	
	std::vector<int> expected = {100, -100, 7};
	boost::remove_erase_if(expected, [pred = view.filter_pred()](auto i) { return not pred(i); });
	BOOST_CHECK(is_equal_sof(view, expected));
}


BOOST_AUTO_TEST_CASE(view_base_test)
{
	using container = viewed::hash_container_base<int>;
	using view = viewed::view_base<container>;

	test_aue<container, view>();
}

BOOST_AUTO_TEST_CASE(view_qtbase_test)
{
	using container = viewed::hash_container_base<int>;
	using view = viewed::view_qtbase<container>;
	using qtview = simple_qtmodel<view>;

	test_aue<container, qtview>();
}

BOOST_AUTO_TEST_CASE(sfview_qtbase_test)
{
	using container = viewed::hash_container_base<int>;
	using asc_nf_view = viewed::sfview_qtbase<container, std::less<int>, no_filter>;
	using asc_nf_qtview = simple_qtmodel<asc_nf_view>;

	using asc_odd_view = viewed::sfview_qtbase<container, std::less<int>, odd_filter>;
	using asc_odd_qtview = simple_qtmodel<asc_odd_view>;

	using desc_odd_view = viewed::sfview_qtbase<container, std::greater<int>, odd_filter>;
	using desc_odd_qtview = simple_qtmodel<desc_odd_view>;

	test_aue_sof<container, asc_nf_qtview>();
	test_aue_sof<container, asc_odd_qtview>();
	test_aue_sof<container, desc_odd_qtview>();
}


BOOST_AUTO_TEST_CASE(sfview_qtbase_presistance_test)
{
	using container_type = viewed::hash_container_base<int>;
	using view_type = simple_qtmodel<
		viewed::sfview_qtbase<container_type, std::less<int>, odd_filter>
	>;

	container_type cont;
	view_type view {&cont};
	view.init();

	std::vector<int> assign_batch1 = {10, 15, 1, 25, 100};
	std::vector<int> upsert_batch = {1, -101};
	std::vector<int> assign_batch2 = {100, 25, 200, -101, 7};

	cont.assign(assign_batch1.begin(), assign_batch1.end());
	QPersistentModelIndex idx = view.index(0);

	BOOST_CHECK(idx.data().toInt() == 1);
	BOOST_CHECK(idx.row() == 0);

	cont.upsert(upsert_batch.begin(), upsert_batch.end());
	BOOST_CHECK(view.index(0).data().toInt() == -101);
	BOOST_CHECK(idx.data().toInt() == 1);
	BOOST_CHECK(idx.row() == 1);

	cont.assign(assign_batch2.begin(), assign_batch2.end());
	BOOST_CHECK(idx.isValid() == false);
	BOOST_CHECK(view.index(0).data().toInt() == -101);
	BOOST_CHECK(view.index(1).data().toInt() == 7);
}
