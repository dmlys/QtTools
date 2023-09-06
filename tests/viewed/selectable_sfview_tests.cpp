#include <boost/test/unit_test.hpp>

#include <boost/mp11/mpl_list.hpp>
#include <boost/mp11/mpl.hpp>

#include <viewed/hash_container.hpp>
#include <viewed/ordered_container.hpp>
#include <viewed/selectable_sfview_qtbase.hpp>

#include <ext/range/pretty_printers.hpp>

namespace
{
	template <class view_type>
	class simple_qtmodel :
		public QAbstractTableModel,
		public view_type
	{
	public:
		using base_type = view_type;
		using container_type = typename view_type::container_type;

	public:
		QVariant data(const QModelIndex & idx, int role) const override;
		int rowCount(const QModelIndex & parent = QModelIndex()) const override { return static_cast<int>(this->size()); }
		int columnCount(const QModelIndex & parent = QModelIndex()) const override { return 2; }
		void selectRow(int row, bool selected);
		
	public:
		simple_qtmodel(std::shared_ptr<container_type> cont)
			: base_type(std::move(cont)) { }
	};
	
	template <class view_type>
	void simple_qtmodel<view_type>::selectRow(int row, bool selected)
	{
		auto it = this->begin() + row;
		this->select_and_notify(it, selected);
	}
	
	template <class view_type>
	QVariant simple_qtmodel<view_type>::data(const QModelIndex & idx, int role) const
	{
		int row = idx.row();
		int column = idx.column();
		
		auto val = *this->m_store[row];
		
		switch (role)
		{
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				return column == 0 ? QVariant::fromValue(QString()) : QVariant::fromValue(val);
					
			case Qt::CheckStateRole:
				if (column == 0)
				{
					auto it = view_type::begin() + row;
					return view_type::is_selected(it) ? Qt::Checked : Qt::Unchecked;
				}
				else
			default: return {};
		}
	}
	

	struct odd_filter
	{
		bool operator()(int i) const noexcept { return i % 2; }
		operator bool() const noexcept { return true; }
	};

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
	
	using container = viewed::hash_container<int>;
	using view_list = boost::mp11::mp_list<
		viewed::selectable_sfview_qtbase<container, std::less<int>, odd_filter>
	>;
	
} // 'anonymous' namespace

BOOST_AUTO_TEST_SUITE(view_tests)

BOOST_AUTO_TEST_CASE_TEMPLATE(selectable_sfview_test, view_type, view_list)
{
	using container_type = typename view_type::container_type;
	using view_model_type = simple_qtmodel<view_type>;
	constexpr int check_col = 0;
	constexpr int data_col = 1;
	
	std::shared_ptr<container_type> cont_ptr = std::make_shared<container_type>();
	container_type & cont = *cont_ptr;
	view_model_type view(cont_ptr);
	
	std::vector<int> assign_batch1 = {10, 15, 1, 25, 100};
	std::vector<int> upsert_batch = {1, -101};
	std::vector<int> assign_batch2 = {100, 25, 200, -101, 7, 13};

	// after assignment sorting and filtering - view holds [1, 15, 25]
	cont.assign(assign_batch1.begin(), assign_batch1.end());
	BOOST_REQUIRE_EQUAL(view.size(), 3);
	
	view.selectRow(0, true); 
	view.selectRow(2, true);
	
	//using ext::pretty_printers::operator<<;
	//std::cout << view << std::endl;
	
	QPersistentModelIndex idx00 = view.index(0, check_col);
	QPersistentModelIndex idx01 = view.index(0, data_col);
	QPersistentModelIndex idx20 = view.index(2, check_col);
	QPersistentModelIndex idx21 = view.index(2, data_col);	
	
	BOOST_CHECK_EQUAL(qvariant_cast<Qt::CheckState>(idx00.data(Qt::CheckStateRole)), Qt::Checked);
	BOOST_CHECK_EQUAL(qvariant_cast<Qt::CheckState>(idx20.data(Qt::CheckStateRole)), Qt::Checked);
	BOOST_CHECK_EQUAL(idx01.data().toInt(), 1);
	BOOST_CHECK_EQUAL(idx21.data().toInt(), 25);

	// after assignment sorting and filtering - view holds [-101, 1, 15, 25]
	cont.upsert(upsert_batch.begin(), upsert_batch.end());
	BOOST_CHECK_EQUAL(view.index(0, data_col).data().toInt(), -101);
	BOOST_CHECK_EQUAL(view.index(0, check_col).data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Unchecked);

	BOOST_CHECK_EQUAL(idx01.row(), 1);
	BOOST_CHECK_EQUAL(idx01.data().toInt(), 1);
	BOOST_CHECK_EQUAL(idx00.data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	BOOST_CHECK_EQUAL(idx21.row(), 3);
	BOOST_CHECK_EQUAL(idx21.data().toInt(), 25);
	BOOST_CHECK_EQUAL(idx20.data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	// this will move checked elements at the begining and algorithm is stable: [1, 25, ...]
	view.partition_by_selection();
	BOOST_CHECK_EQUAL(view.index(0, data_col).data().toInt(), 1);
	BOOST_CHECK_EQUAL(view.index(0, check_col).data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	BOOST_CHECK_EQUAL(idx01.row(), 0);
	BOOST_CHECK_EQUAL(idx01.data().toInt(), 1);
	BOOST_CHECK_EQUAL(idx00.data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	BOOST_CHECK_EQUAL(idx21.row(), 1);
	BOOST_CHECK_EQUAL(idx21.data().toInt(), 25);
	BOOST_CHECK_EQUAL(idx20.data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	// this will remove element with val = 1, so view become: [25, ...]
	cont.assign(assign_batch2.begin(), assign_batch2.end());
	BOOST_CHECK_EQUAL(idx00.isValid(), false);
	BOOST_CHECK_EQUAL(idx01.isValid(), false);
	BOOST_CHECK_EQUAL(idx20.isValid(), true);
	BOOST_CHECK_EQUAL(idx21.isValid(), true);	
	
	BOOST_CHECK_EQUAL(view.index(0, data_col).data().toInt(), 25);
	BOOST_CHECK_EQUAL(view.index(0, check_col).data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	BOOST_CHECK_EQUAL(idx21.row(), 0);
	BOOST_CHECK_EQUAL(idx21.data().toInt(), 25);
	BOOST_CHECK_EQUAL(idx20.data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);
	
	// after sorting and filtering view should hold: [-101, 7, 13, 25]
	view.reset_partitioning(true);
	BOOST_REQUIRE_EQUAL(view.size(), 4);
	
	BOOST_CHECK_EQUAL(view.index(0, data_col).data().toInt(), -101);
	BOOST_CHECK_EQUAL(view.index(0, check_col).data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Unchecked);
	BOOST_CHECK_EQUAL(view.index(3, data_col).data().toInt(), 25);
	BOOST_CHECK_EQUAL(view.index(3, check_col).data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);

	BOOST_CHECK_EQUAL(idx00.isValid(), false);
	BOOST_CHECK_EQUAL(idx01.isValid(), false);
	BOOST_CHECK_EQUAL(idx20.isValid(), true);
	BOOST_CHECK_EQUAL(idx21.isValid(), true);

	BOOST_CHECK_EQUAL(idx21.row(), 3);
	BOOST_CHECK_EQUAL(idx21.data().toInt(), 25);
	BOOST_CHECK_EQUAL(idx20.data(Qt::CheckStateRole).template value<Qt::CheckState>(), Qt::Checked);	
}

BOOST_AUTO_TEST_SUITE_END()
