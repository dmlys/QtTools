#include "TestTreeModel.hqt"
#include <QtTools/ToolsBase.hpp>

template <class Entity, class Type, Type Entity::*member, class Pred>
bool test_entity_sftree_traits::test_tree_sorter::compare_entity(const Entity & e1, const Entity & e2) noexcept
{
	Pred pred;
	const auto & v1 = e1.*member;
	const auto & v2 = e2.*member;
	return pred(v1, v2);
}

void test_entity_sftree_traits::test_tree_sorter::reset(unsigned type, Qt::SortOrder order)
{
	switch (type)
	{
	default:
		m_leaf_compare = nullptr;
		m_node_compare = nullptr;
		return;

	case 0:
		m_leaf_compare = order == Qt::AscendingOrder ? &compare_entity<leaf_type, path_type, &leaf_type::filename, std::less<>> : &compare_entity<leaf_type, path_type, &leaf_type::filename, std::greater<>>;
		m_node_compare = order == Qt::AscendingOrder ? &compare_entity<node_type, path_type, &node_type::filename, std::less<>> : &compare_entity<node_type, path_type, &node_type::filename, std::greater<>>;
		return;

	case 1:
		m_leaf_compare = order == Qt::AscendingOrder ? &compare_entity<leaf_type, std::string, &leaf_type::sometext, std::less<>> : &compare_entity<leaf_type, std::string, &leaf_type::sometext, std::greater<>>;
		m_node_compare = order == Qt::AscendingOrder ? &compare_entity<node_type, std::string, &node_type::sometext, std::less<>> : &compare_entity<node_type, std::string, &node_type::sometext, std::greater<>>;

	case 2:
		m_leaf_compare = order == Qt::AscendingOrder ? &compare_entity<leaf_type, int, &leaf_type::int_value, std::less<>> : &compare_entity<leaf_type, int, &leaf_type::int_value, std::greater<>>;
		m_node_compare = order == Qt::AscendingOrder ? &compare_entity<node_type, int, &node_type::int_value, std::less<>> : &compare_entity<node_type, int, &node_type::int_value, std::greater<>>;
		return;

	}
}

void test_entity_sftree_traits::test_tree_sorter::reset(viewed::nosort_type)
{
	m_leaf_compare = nullptr;
	m_node_compare = nullptr;
}

viewed::refilter_type test_entity_sftree_traits::test_tree_filter::set_expr(QString expr)
{
	expr = expr.trimmed();

	if (expr.compare(m_filterStr, Qt::CaseInsensitive) == 0)
		return viewed::refilter_type::same;

	if (expr.startsWith(m_filterStr, Qt::CaseInsensitive))
	{
		m_filterStr = expr;
		return viewed::refilter_type::incremental;
	}
	else
	{
		m_filterStr = expr;
		return viewed::refilter_type::full;
	}
}

bool test_entity_sftree_traits::test_tree_filter::matches(const pathview_type & val) const noexcept
{
	return val.contains(m_filterStr, Qt::CaseInsensitive);
}

void TestTreeModelBase::recalculate_page(page_type & page)
{
	page.node.int_value = 0;
	for (auto & child : page.children)
	{
		auto visitor = [](auto * item) { return item->int_value; };
		page.node.int_value += viewed::visit(node_accessor(visitor), child);
	}
}

void TestTreeModelBase::SortBy(int section, Qt::SortOrder order)
{
	section = ViewToMetaIndex(section);
	base_type::sort_by(section, order);
}

void TestTreeModelBase::FilterBy(QString expr)
{
	base_type::filter_by(expr);
}

struct any_from_element
{
	auto operator()(const test_tree_entity * ptr) const { return QVariant::fromValue(ptr); }
	//auto operator()(const test_tree_node   * ptr) const { return QVariant::fromValue(ptr); }
};

QVariant TestTreeModelBase::GetEntity(const QModelIndex & index) const
{
	if (not index.isValid()) return QVariant();

	const auto & val = get_ielement_ptr(index);
	return viewed::visit(node_accessor(any_from_element()), val);
}

QVariant TestTreeModelBase::GetItem(const QModelIndex & index) const
{
	if (not index.isValid()) return QVariant();

	const auto & val = get_ielement_ptr(index);
	const auto meta_index = ViewToMetaIndex(index.column());

	auto visitor = [meta_index](auto * ptr) -> QVariant
	{
		switch (meta_index)
		{
			case 0:  return QVariant::fromValue(::get_name(&ptr->filename).toString());
			case 1:  return QVariant::fromValue(ptr->sometext);
			case 2:  return QVariant::fromValue(ptr->int_value);
			default: return {};
		}
	};

	return viewed::visit(node_accessor(visitor), val);
}

int TestTreeModelBase::FullRowCount(const QModelIndex & index) const
{
	if (not index.isValid()) return 0;

	const auto * page = get_page(index);
	return page->children.size();
}
