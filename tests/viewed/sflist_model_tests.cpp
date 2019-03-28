#include <boost/test/unit_test.hpp>
#include <boost/mp11.hpp>
#include <boost/mp11/mpl.hpp>

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptors.hpp>

#include <viewed/sflist_model_qtbase.hpp>

namespace
{
	template <class Type, class Sorter, class Filter>
	class sflist_model :
		public QAbstractListModel,
		public viewed::sflist_model_qtbase<Type, Sorter, Filter>
	{
		using self_type = sflist_model;
		using view_type = viewed::sflist_model_qtbase<Type, Sorter, Filter>;

	public:
		QVariant data(const QModelIndex & idx, int role) const override {
			return QVariant::fromValue(view_type::operator[](idx.row()));
		}
		int rowCount(const QModelIndex & parent = QModelIndex()) const override { return static_cast<int>(this->m_nvisible); }

	public:
		using QAbstractListModel::QAbstractListModel;
	};


	struct greater_filter
	{
		int limit = INT_MIN;

		operator bool() const noexcept { return limit > INT_MIN; }
		bool operator()(int val) const noexcept { return val >= limit; }
		viewed::refilter_type set_expr(int limit)
		{
			auto old_limit = std::exchange(this->limit, limit);
			if (limit == old_limit) return viewed::refilter_type::same;
			if (limit >  old_limit) return viewed::refilter_type::incremental;

			return viewed::refilter_type::full;
		}
	};
} // 'anonymous' namespace


BOOST_AUTO_TEST_SUITE(sflist_model_tests)

BOOST_AUTO_TEST_CASE(simple_tests)
{
	sflist_model<int, std::less<>, viewed::null_filter> model;

	auto assign_data = {15, 10, 1, 25, 100, 256};
	auto append_data = {15, 10, 900, -200, -100, 0};

	model.assign(assign_data);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 1);

	QPersistentModelIndex idx1 = model.index(0);
	BOOST_CHECK(idx1.isValid() and idx1.row() == 0);

	model.append(append_data);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), -200);

	QPersistentModelIndex idx2 = model.index(0);
	BOOST_CHECK(idx2.isValid() and idx2.row() == 0);

	BOOST_CHECK_EQUAL(idx1.data().toInt(), 1);
	BOOST_CHECK(idx1.isValid());
	BOOST_CHECK_EQUAL(idx1.row(), 3); // moved to 3 position

	model.assign(assign_data);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 1);

	// assignment always resets model
	BOOST_CHECK(not idx1.isValid());  // gone
	BOOST_CHECK(not idx2.isValid());  // gone
}

BOOST_AUTO_TEST_CASE(filter_tests)
{
	sflist_model<int, std::variant<std::less<>, std::greater<>>, greater_filter> model;

	auto assign_data = {33, 50, -20, 1, 0, 100, -100, 25, 12};
	decltype(assign_data) expected_data;

	model.filter_by(25);
	model.sort_by(std::greater<>());
	model.append(assign_data);

	expected_data = {100, 50, 33, 25};
	BOOST_CHECK_EQUAL_COLLECTIONS(model.begin(), model.end(), expected_data.begin(), expected_data.end());

	QPersistentModelIndex idx = model.index(0);
	BOOST_CHECK_EQUAL(idx.data().toInt(), 100);

	model.filter_by(50);
	model.sort_by(std::less<>());

	BOOST_CHECK_EQUAL(idx.data().toInt(), 100);
	BOOST_CHECK_EQUAL(idx.row(), 1);

	model.erase(model.begin(), model.end());
	BOOST_CHECK(not idx.isValid());
	BOOST_CHECK(model.empty());
}

BOOST_AUTO_TEST_SUITE_END()

