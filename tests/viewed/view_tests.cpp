#include <boost/test/unit_test.hpp>

#include <boost/mp11/mpl_list.hpp>
#include <boost/mp11/mpl.hpp>

#include <viewed/hash_container.hpp>
#include <viewed/ordered_container.hpp>
#include <viewed/sfview_qtbase.hpp>

namespace
{
	template <class view_type>
	class simple_qtmodel :
		public QAbstractListModel,
		public view_type
	{
	public:
		using base_type = view_type;
		using container_type = typename view_type::container_type;

	public:
		QVariant data(const QModelIndex & idx, int role) const override { return QVariant::fromValue(*this->m_store[idx.row()]); }
		int rowCount(const QModelIndex & parent = QModelIndex()) const override { return static_cast<int>(this->size()); }

	public:
		simple_qtmodel(std::shared_ptr<container_type> cont)
			: base_type(std::move(cont)) { }
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
} // 'anonymous' namespace

BOOST_AUTO_TEST_SUITE(view_tests)

using aue_container = viewed::hash_container<int>;
using aue_test_list = boost::mp11::mp_list<
	viewed::view_base<aue_container>,
	simple_qtmodel<viewed::view_qtbase<aue_container>>
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(assign_update_erase_test, view_type, aue_test_list)
{
	using container_type = typename view_type::container_type;
	std::shared_ptr<container_type> cont_ptr = std::make_shared<container_type>();
	container_type & cont = *cont_ptr;
	view_type view(cont_ptr);

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

using aue_container = viewed::hash_container<int>;
using aue_sof_test_list = boost::mp11::mp_list<
	viewed::sfview_qtbase<aue_container, std::less<int>, viewed::null_filter>,
	viewed::sfview_qtbase<aue_container, std::less<int>, odd_filter>,
	viewed::sfview_qtbase<aue_container, std::greater<int>, odd_filter>
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(sfview_qtbase_test, view_type, aue_sof_test_list)
{
	using container_type = typename view_type::container_type;
	using view_model_type = simple_qtmodel<view_type>;
	
	std::shared_ptr<container_type> cont_ptr = std::make_shared<container_type>();
	container_type & cont = *cont_ptr;
	view_model_type view(cont_ptr);	

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
	BOOST_CHECK(is_equal_sof(view, expected));
}

using aue_container = viewed::hash_container<int>;
using persistance_view_test_list = boost::mp11::mp_list<
	viewed::sfview_qtbase<aue_container, std::less<int>, viewed::null_filter>,
	viewed::sfview_qtbase<aue_container, std::less<int>, odd_filter>
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(sfview_qtbase_presistance_test, view_type, persistance_view_test_list)
{
	using container_type = typename view_type::container_type;
	using view_model_type = simple_qtmodel<view_type>;

	std::shared_ptr<container_type> cont_ptr = std::make_shared<container_type>();
	container_type & cont = *cont_ptr;
	view_model_type view(cont_ptr);

	std::vector<int> assign_batch1 = {10, 15, 1, 25, 100};
	std::vector<int> upsert_batch = {1, -101};
	std::vector<int> assign_batch2 = {100, 25, 200, -101, 7};

	cont.assign(assign_batch1.begin(), assign_batch1.end());
	QPersistentModelIndex idx = view.index(0);

	BOOST_CHECK_EQUAL(idx.data().toInt(), 1);
	BOOST_CHECK_EQUAL(idx.row(), 0);

	cont.upsert(upsert_batch.begin(), upsert_batch.end());
	BOOST_CHECK_EQUAL(view.index(0).data().toInt(), -101);
	BOOST_CHECK_EQUAL(idx.data().toInt(), 1);
	BOOST_CHECK_EQUAL(idx.row(), 1);

	cont.assign(assign_batch2.begin(), assign_batch2.end());
	BOOST_CHECK_EQUAL(idx.isValid(), false);
	BOOST_CHECK_EQUAL(view.index(0).data().toInt(), -101);
	BOOST_CHECK_EQUAL(view.index(1).data().toInt(), 7);
}

BOOST_AUTO_TEST_SUITE_END()
