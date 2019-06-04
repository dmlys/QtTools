#include <boost/test/unit_test.hpp>
#include <boost/mp11.hpp>
#include <boost/mp11/mpl.hpp>

#include <viewed/hash_container.hpp>
#include <viewed/ordered_container.hpp>

#include <viewed/sftree_model_qtbase.hpp>
#include <viewed/sftree_view_qtbase.hpp>
#include <viewed/sftree_string_traits.hpp>

#include <QtTools/ToolsBase.hpp>

namespace
{
	template <class Sorter, class Filter>
	struct tree_traits : viewed::sftree_string_traits
	{
		using base_type = viewed::sftree_string_traits;

		using leaf_type = path_type;
		using node_type = path_type;

		// those must be static
		void set_name(node_type & node, const pathview_type & path, const pathview_type & name) const { node = name; }
		pathview_type get_name(const leaf_type & leaf) const { return base_type::get_name(leaf); }
		pathview_type get_path(const leaf_type & leaf) const { return leaf; }

		using sort_pred_type   = Sorter;
		using filter_pred_type = Filter;
	};

	template <class Sorter, class Filter>
	class tree_model_base : public viewed::sftree_facade_qtbase<tree_traits<Sorter, Filter>, QAbstractListModel>
	{
		using base_type = viewed::sftree_facade_qtbase<tree_traits<Sorter, Filter>, QAbstractListModel>;

	protected:
		virtual void recalculate_page(typename base_type::page_type & page) override {}

	public:
		QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override { return ToQString(this->get_name(this->get_ielement_ptr(index))); }

	public:
		using base_type::base_type;
	};


	using container_type = viewed::hash_container<std::string>;

	template <class Sorter, class Filter> using tree_model = viewed::sftree_model_qtbase<tree_model_base<Sorter, Filter>>;
	template <class Sorter, class Filter> using tree_view_model = viewed::sftree_view_qtbase<container_type, tree_model_base<Sorter, Filter>>;



	struct less_sorter
	{
		using traits = viewed::sftree_string_traits;
		const traits * t = nullptr;

		void set_traits(const traits * t) { this->t = t;}

		template <auto ... vals>
		using hint = std::integer_sequence<unsigned, vals...>;

		bool operator()(const std::string_view & s1, const std::string_view & s2, hint<viewed::PAGE, viewed::PAGE>) const noexcept { return t->get_name(s1) < t->get_name(s2); }
		bool operator()(const std::string_view & s1, const std::string_view & s2, hint<viewed::PAGE, viewed::LEAF>) const noexcept { return true; }
		bool operator()(const std::string_view & s1, const std::string_view & s2, hint<viewed::LEAF, viewed::PAGE>) const noexcept { return false; }
		bool operator()(const std::string_view & s1, const std::string_view & s2, hint<viewed::LEAF, viewed::LEAF>) const noexcept { return t->get_name(s1) < t->get_name(s2); }
	};

	struct greater_sorter : less_sorter
	{
		template <class ... Args>
		bool operator()(const std::string_view & s1, const std::string_view & s2, Args && ... args) const noexcept
		{ return less_sorter::operator()(s2, s1, std::forward<Args>(args)...); }
	};

	struct filter
	{
		using traits = viewed::sftree_string_traits;
		const traits * t = nullptr;

		void set_traits(const traits * t) { this->t = t;}

		std::string substr;

		bool operator()(const std::string & str) const noexcept { return t->get_name(str).find(substr) != str.npos; }
		explicit operator bool() const noexcept { return not substr.empty(); }

		auto set_expr(std::string substr) { this->substr = std::move(substr); return viewed::refilter_type::full; }
	};



	/************************************************************************/
	/*                     helper traits for tests                          */
	/************************************************************************/

	template <class Type>
	struct traits;

	template <class Sorter, class Filter>
	struct traits<tree_model<Sorter, Filter>>
	{
		using model_type = tree_model<Sorter, Filter>;

		static model_type create() { return model_type(); }

		template <class ... Args> static void assign(model_type & model, Args && ... args) { model.assign(std::forward<Args>(args)...); }
		template <class ... Args> static void upsert(model_type & model, Args && ... args) { model.upsert(std::forward<Args>(args)...); }

		template <class Type> static void assign(model_type & model, std::initializer_list<Type> data) { assign(model, data.begin(), data.end()); }
		template <class Type> static void upsert(model_type & model, std::initializer_list<Type> data) { upsert(model, data.begin(), data.end()); }

		template <class NewSorter> using rebind_sorter = tree_model<NewSorter, Filter>;
		template <class NewFilter> using rebind_filter = tree_model<Sorter, NewFilter>;
		template <class NewSorter, class NewFilter> using rebind = tree_model<NewSorter, NewFilter>;
	};

	template <class Sorter, class Filter>
	struct traits<tree_view_model<Sorter, Filter>>
	{
		using model_type = tree_view_model<Sorter, Filter>;
		using container_type = ::container_type;

		static model_type create()
		{
			auto container = std::make_shared<container_type>();
			return model_type(std::move(container));
		}

		template <class ... Args>
		static void assign(model_type & model, Args && ... args)
		{
			const auto & container = model.get_owner();
			container->assign(std::forward<Args>(args)...);
		}

		template <class ... Args>
		static void upsert(model_type & model, Args && ... args)
		{
			const auto & container = model.get_owner();
			container->upsert(std::forward<Args>(args)...);
		}

		template <class Type> static void assign(model_type & model, std::initializer_list<Type> data) { assign(model, data.begin(), data.end()); }
		template <class Type> static void upsert(model_type & model, std::initializer_list<Type> data) { upsert(model, data.begin(), data.end()); }

		template <class NewSorter> using rebind_sorter = tree_view_model<NewSorter, Filter>;
		template <class NewFilter> using rebind_filter = tree_view_model<Sorter, NewFilter>;
		template <class NewSorter, class NewFilter> using rebind = tree_view_model<NewSorter, NewFilter>;
	};
} // 'anonymous' namespace


using test_types = boost::mp11::mp_list<tree_model<less_sorter, viewed::null_filter>, tree_view_model<less_sorter, viewed::null_filter>>;

BOOST_AUTO_TEST_SUITE(sftree_facade_tests)

BOOST_AUTO_TEST_CASE_TEMPLATE(simple_tests, model_type, test_types)
{
	auto assign_data =
	{
	    "folder-1/file1.txt",
	    "folder-1/file2.txt",
	    "root-file1",
	    "root-file2",
	};

	auto upsert_data =
	{
	    "folder-0/test1.txt",
	    "folder-0/test2.txt",
		"folder-1/file3.txt",
	    "folder-3/foo.ttt",
	};

	using traits = ::traits<model_type>;
	model_type model = traits::create();
	traits::assign(model, assign_data);

	QPersistentModelIndex f1_idx   = model.find_element("folder-1");
	QPersistentModelIndex ff11_idx = model.find_element(f1_idx, "file1.txt");
	QPersistentModelIndex ff12_idx = model.find_element("folder-1/file2.txt");

	BOOST_CHECK(f1_idx.isValid());
	BOOST_CHECK(ff11_idx.isValid());
	BOOST_CHECK(ff12_idx.isValid());
	BOOST_CHECK(f1_idx == ff12_idx.parent());

	BOOST_CHECK_EQUAL(f1_idx.row(), 0);
	BOOST_CHECK_EQUAL(ff11_idx.row(), 0);
	BOOST_CHECK_EQUAL(ff12_idx.row(), 1);

	BOOST_CHECK_EQUAL(f1_idx.data().toString(), "folder-1");
	BOOST_CHECK_EQUAL(ff11_idx.data().toString(), "file1.txt");
	BOOST_CHECK_EQUAL(ff12_idx.data().toString(), "file2.txt");


	traits::upsert(model, upsert_data);

	BOOST_CHECK(f1_idx.isValid());
	BOOST_CHECK(ff11_idx.isValid());
	BOOST_CHECK(ff12_idx.isValid());

	BOOST_CHECK_EQUAL(f1_idx.row(), 1);
	BOOST_CHECK_EQUAL(ff11_idx.row(), 0);
	BOOST_CHECK_EQUAL(ff12_idx.row(), 1);

	BOOST_CHECK_EQUAL(f1_idx.data().toString(), "folder-1");
	BOOST_CHECK_EQUAL(ff11_idx.data().toString(), "file1.txt");
	BOOST_CHECK_EQUAL(ff12_idx.data().toString(), "file2.txt");
}

BOOST_AUTO_TEST_CASE(filter_tests)
{
	auto data1 =
	{
		"folder/file.txt",
		"folder/test.sft",
		"folder/inner/file.txt",
	};

	tree_model<less_sorter, filter> model;
	model.filter_by("folder");
	model.assign(data1);

	QPersistentModelIndex idx;
	idx = model.find_element("folder/inner");
	BOOST_CHECK(not idx.isValid());

	model.filter_by("inner");

	idx = model.find_element("folder/inner");
	BOOST_CHECK(idx.isValid());

	model.filter_by("file.txt");
	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(idx.data().toString(), "inner");
}

BOOST_AUTO_TEST_CASE(sort_tests)
{
	auto data =
	{
		"folder/file1.txt",
	    "folder/file2.txt",
	    "folder/inner/file1.txt",
	};

	tree_model<std::variant<less_sorter, greater_sorter>, filter> model;
	model.assign(data);

	QPersistentModelIndex idx = model.find_element("folder/file1.txt");
	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(idx.row(), 1);
	BOOST_CHECK_EQUAL(idx.data().toString(), "file1.txt");

	model.sort_by(greater_sorter());

	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(idx.row(), 2);
	BOOST_CHECK_EQUAL(idx.data().toString(), "file1.txt");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(unexpected_node, model_type, test_types)
{
	// By definition sftree_facade expects only leafs as input, nodes are created internally.
	// There is a case were we can pass data which conflicts with those statements.
	// Bellow "folder" is a node, how those sneaky boys treated - depends on is_child method from traits:
	//   They can be completely ignored or inserted as leafs(former is probably beteer and easier)

	auto assign_data =
	{
	    "folder/file",
	    "folder", // ha! you didn't expected me!
	};

	using traits = ::traits<model_type>;
	model_type model = traits::create();
	traits::assign(model, assign_data);

	BOOST_CHECK_EQUAL(model.rowCount(), 1);

	// folder is skipped and not appears in model, overall such input data should be avoided.
	auto folder_idx = model.index(0, 0);
	BOOST_CHECK_EQUAL(model.rowCount(folder_idx), 1);
	BOOST_CHECK_EQUAL(folder_idx.data().toString(), "folder");
	BOOST_CHECK_EQUAL(folder_idx.child(0, 0).data().toString(), "file");
}

//BOOST_AUTO_TEST_CASE_TEMPLATE(unexpected_node_inserted, model_type, test_types)
//{
//	// By definition sftree_facade expects only leafs as input, nodes are created internally.
//	// There is a case were we can pass data which conflicts with those statements.
//	// Bellow "folder" is a node, this class still handles it as a leaf, and not in a very intuitive matter.
//	auto assign_data =
//	{
//	    "folder/file",
//	    "folder", // ha! you didn't expected me!
//	};

//	using traits1 = ::traits<model_type>;
//	auto model1 = traits1::create();
//	traits1::assign(model1, assign_data);

//	BOOST_CHECK_EQUAL(model1.rowCount(), 1);

//	// folder after processing appears internally as "folder/folder" item,
//	// overall such input data should be avoided,
//	// TODO: maybe class should skip those elements...
//	auto folder_idx = model1.index(0, 0);
//	BOOST_CHECK_EQUAL(folder_idx.data().toString(), "folder");
//	BOOST_CHECK_EQUAL(model1.rowCount(folder_idx), 1);

//	BOOST_CHECK_EQUAL(model1.index(0, 0, folder_idx).data().toString(), "file");
//	BOOST_CHECK_EQUAL(model1.index(1, 0, folder_idx).data().toString(), "folder");

//	// now with different sorter, to illustrate class behavior
//	using model_type2 = typename traits1::template rebind_sorter<std::less<>>;
//	using traits2 = ::traits<model_type2>;
//	auto model2 = traits2::create();
//	traits2::assign(model2, assign_data);

//	BOOST_CHECK_EQUAL(model2.rowCount(), 1);

//	folder_idx = model2.index(0, 0);
//	BOOST_CHECK_EQUAL(folder_idx.data().toString(), "folder");
//	BOOST_CHECK_EQUAL(model2.rowCount(folder_idx), 1);

//	// note order is different, it's because std::less<> sees paths as is and folder < folder/file
//	// and sorter sees paths as: folder/folder and folder/file and folder/folder > folder/file
//	BOOST_CHECK_EQUAL(model2.index(0, 0, folder_idx).data().toString(), "folder");
//	BOOST_CHECK_EQUAL(model2.index(1, 0, folder_idx).data().toString(), "file");
//}

BOOST_AUTO_TEST_CASE_TEMPLATE(leaf_node_transormation, model_type, test_types)
{
	auto data1 = { "folder", };      // folder is a leaf
	auto data2 = { "folder/file", }; // and now it's a node

	using traits = ::traits<model_type>;
	model_type model = traits::create();
	traits::upsert(model, data1);

	auto idx = model.index(0, 0);
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
	BOOST_CHECK_EQUAL(model.rowCount(idx), 0);

	traits::upsert(model, data2);
	idx = model.index(0, 0);
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
	BOOST_CHECK_EQUAL(model.rowCount(idx), 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(node_leaf_transformation, model_type, test_types)
{
	auto data1 = { "folder/file", }; // folder is a node
	auto data2 = { "folder", };      // and now it's a leaf

	using traits = ::traits<model_type>;
	model_type model = traits::create();
	traits::upsert(model, data1);

	auto idx = model.index(0, 0);
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
	BOOST_CHECK_EQUAL(model.rowCount(idx), 1);

	traits::upsert(model, data2);
	idx = model.index(0, 0);
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
	BOOST_CHECK_EQUAL(model.rowCount(idx), 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(duplicate_tests, model_type, test_types)
{
	auto data1 = {"folder", "folder", "folder"};
	auto data2 = {"folder", "folder", "folder"};

	using traits = ::traits<model_type>;
	model_type model = traits::create();
	traits::assign(model, data1);

	QPersistentModelIndex idx = model.index(0, 0);
	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
	BOOST_CHECK_EQUAL(model.rowCount(idx), 0);


	traits::upsert(model, data2);

	BOOST_CHECK(idx.isValid());
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
	BOOST_CHECK_EQUAL(model.rowCount(idx), 0);
}

BOOST_AUTO_TEST_CASE(model_view_reset)
{
	auto data =
	{
		"folder/file1.txt",
	    "folder/file2.txt",
	    "folder/inner/file1.txt",
	};

	auto container = std::make_shared<container_type>();
	tree_view_model<std::variant<less_sorter, greater_sorter>, filter> model(container);

	container->assign(data.begin(), data.end());
	BOOST_CHECK_EQUAL(model.rowCount(), 1);

	model.reinit_view_and_notify();
	BOOST_CHECK_EQUAL(model.rowCount(), 1);
}

BOOST_AUTO_TEST_SUITE_END()
