#include <boost/test/unit_test.hpp>
#include <boost/mp11.hpp>
#include <boost/mp11/mpl.hpp>

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptors.hpp>

#include <viewed/sfset_model_qtbase.hpp>

namespace
{
	template <class Type, class Sorter, class Filter>
	class sfset_model :
		public QAbstractListModel,
		public viewed::sfset_model_qtbase<Type, Sorter, Filter>
	{
		using self_type = sfset_model;
		using view_type = viewed::sfset_model_qtbase<Type, Sorter, Filter>;

	public:
		QVariant data(const QModelIndex & idx, int role) const override { return QVariant::fromValue(view_type::operator[](idx.row())); }
		int rowCount(const QModelIndex & parent = QModelIndex()) const override { return static_cast<int>(this->m_nvisible); }

	public:
		using QAbstractListModel::QAbstractListModel;
	};


	struct nratio_filter
	{
		int ratio = 0;

		operator bool() const noexcept { return ratio != 0; }
		bool operator()(int val) const noexcept { return val % ratio != 0; }
		viewed::refilter_type set_expr(int ratio) { this->ratio = ratio; return viewed::refilter_type::full; }

		nratio_filter() = default;
		nratio_filter(int ratio) : ratio(ratio) {}
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


BOOST_AUTO_TEST_SUITE(sfset_model_tests)

BOOST_AUTO_TEST_CASE(simple_tests)
{
	sfset_model<int, std::less<>, viewed::null_filter> model;

	auto assign_data = {15, 10, 1, 25, 100, 256};
	auto upsert_data = {15, 10, 900, -200, -100, 0};

	model.assign(assign_data);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 1);

	QPersistentModelIndex idx1 = model.index(0);
	BOOST_CHECK(idx1.isValid() and idx1.row() == 0);

	model.upsert(upsert_data);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), -200);

	QPersistentModelIndex idx2 = model.index(0);
	BOOST_CHECK(idx2.isValid() and idx2.row() == 0);

	BOOST_CHECK_EQUAL(idx1.data().toInt(), 1);
	BOOST_CHECK(idx1.isValid());
	BOOST_CHECK_EQUAL(idx1.row(), 3); // moved to 3 position

	model.assign(assign_data);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 1);

	BOOST_CHECK(idx1.isValid());
	BOOST_CHECK_EQUAL(idx1.row(), 0); // returned to 0 position
	BOOST_CHECK(not idx2.isValid());  // gone

	model.erase(1);
	BOOST_CHECK(not idx1.isValid()); // gone
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 10);

	auto erase_data = {10, 25};
	model.erase(erase_data);
	BOOST_CHECK_EQUAL(model.rowCount(), 3);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 15);

	model.erase(model.begin() + 1, model.begin() + 2);
	BOOST_CHECK_EQUAL(model.rowCount(), 2);
	BOOST_CHECK_EQUAL(model.index(0).data().toInt(), 15);
	BOOST_CHECK_EQUAL(model.index(1).data().toInt(), 256);

	model.clear();
	BOOST_CHECK_EQUAL(model.rowCount(), 0);
}

BOOST_AUTO_TEST_CASE(simple_filter_tests)
{
	using model_type = sfset_model<int, std::greater<>, nratio_filter>;
	model_type model;

	// reversed         99, 15, 10, 0, -1, -25, -80 in order
	auto assign_data = {15, 10, -1, 0, -25, 99, -80, };
	model.assign(assign_data);

	BOOST_CHECK_EQUAL(model.rowCount(), assign_data.size());

	QPersistentModelIndex idx = model.index(4);
	BOOST_CHECK_EQUAL(idx.data().toInt(), -1);

	model.filter_by(2);
	BOOST_CHECK(std::all_of(model.begin(), model.end(), nratio_filter(2)));

	auto expected = {99, 15, -1, -25, };
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), model.begin(), model.end());

	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(idx.row(), 2);

	model.filter_by(3);
	BOOST_CHECK(std::all_of(model.begin(), model.end(), nratio_filter(3)));

	expected = {10, -1, -25, -80, };
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), model.begin(), model.end());
	// check reverse iterators
	expected = {-80, -25, -1, 10, };
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), model.rbegin(), model.rend());

	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(idx.row(), 1);

	model.filter_by(0);
	expected = {99, 15, 10, 0, -1, -25, -80, };
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), model.begin(), model.end());
	// check reverse iterators
	expected = {-80, -25, -1, 0, 10, 15, 99, };
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), model.rbegin(), model.rend());

	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(idx.row(), 4);
}

BOOST_AUTO_TEST_CASE(advanced_filter_tests)
{
	using model_type = sfset_model<int, std::variant<std::less<>, std::greater<>>, greater_filter>;
	model_type model;

	auto assign_data = {33, 50, -20, 1, 0, 100, -100, 25, 12};
	decltype(assign_data) expected_data;

	model.assign(assign_data);

	expected_data = {-100, -20, 0, 1, 12, 25, 33, 50, 100};
	BOOST_CHECK_EQUAL_COLLECTIONS(model.begin(), model.end(), expected_data.begin(), expected_data.end());

	QPersistentModelIndex idx = model.index(7);
	BOOST_CHECK_EQUAL(idx.data().toInt(), 50);

	model.sort_by(std::greater<>());

	expected_data = {100, 50, 33, 25, 12, 1, 0, -20, -100};
	BOOST_CHECK_EQUAL_COLLECTIONS(model.begin(), model.end(), expected_data.begin(), expected_data.end());
	BOOST_CHECK_EQUAL(idx.data().toInt(), 50);
	BOOST_CHECK_EQUAL(idx.row(), 1);

	BOOST_CHECK(model.filter_by(25) == viewed::refilter_type::incremental);
	expected_data = {100, 50, 33, 25};
	BOOST_CHECK_EQUAL_COLLECTIONS(model.begin(), model.end(), expected_data.begin(), expected_data.end());
	BOOST_CHECK_EQUAL(idx.data().toInt(), 50);
	BOOST_CHECK_EQUAL(idx.row(), 1);

	BOOST_CHECK(model.filter_by(100) == viewed::refilter_type::incremental);
	expected_data = {100};
	BOOST_CHECK_EQUAL_COLLECTIONS(model.begin(), model.end(), expected_data.begin(), expected_data.end());
	BOOST_CHECK(not idx.isValid());

	BOOST_CHECK(model.filter_by(INT_MIN) == viewed::refilter_type::full);
	model.sort_by(std::less<>());
	expected_data = {-100, -20, 0, 1, 12, 25, 33, 50, 100};
	BOOST_CHECK_EQUAL_COLLECTIONS(model.begin(), model.end(), expected_data.begin(), expected_data.end());
}

BOOST_AUTO_TEST_SUITE_END()
