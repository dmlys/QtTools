#include <string>

#include <viewed/sftableset_model_qtbase.hpp>

#include <QtWidgets/QTableView>
#include <QtWidgets/QApplication>

struct test_entity
{
	std::string name;
	int volume;
};

struct traits
{
	using value_type = test_entity;
	using key_type = std::string;
	using key_equal_to_type = std::equal_to<>;
	using key_less_type = std::less<>;
	using key_hash_type = std::hash<key_type>;

	using sort_pred_type = viewed::null_sorter;
	using filter_pred_type = viewed::null_filter;

	inline static const key_type & get_key(const test_entity & val) noexcept { return val.name; }
	inline static void update(value_type & val, value_type && newval) { val = std::move(newval); }
	inline static void update(value_type & val, const value_type & newval) { val = newval; }
};

class test_model : public QAbstractTableModel, public viewed::sftableset_model_qtbase<traits>
{
	using model_base = QAbstractTableModel;
	using view_base = viewed::sftableset_model_qtbase<traits>;

public:
	virtual int rowCount(const QModelIndex & parent = QModelIndex()) const override { return m_nvisible; }
	virtual int columnCount(const QModelIndex & parent = QModelIndex()) const override { return 2; }

	virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public:
	using QAbstractTableModel::QAbstractTableModel;
};


QVariant test_model::data(const QModelIndex & index, int role) const
{
	auto & row = m_store[index.row()];

	if (role != Qt::DisplayRole) return {};

	switch (index.column())
	{
		case 0:  return QtTools::ToQString(row.name);
		case 1:  return row.volume;
		default: return {};
	}
}

QVariant test_model::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Vertical)
		return QAbstractTableModel::headerData(section, orientation, role);

	switch (role)
	{
	    case Qt::DisplayRole:
	    case Qt::ToolTipRole:
			if (section == 0) return QStringLiteral("name");
			if (section == 1) return QStringLiteral("volume");

	    default: return {};
	};
}


int main(int argc, char ** argv)
{
	std::vector<test_entity> data =
	{
	    {"first", 1},
	    {"second", 2},
	    {"opla", 3},
	    {"123", 4},
	};

	test_model model;

	QApplication qapp {argc, argv};

	QTableView view;
	view.setModel(&model);

	view.show();

	model.assign(std::move(data));

	return qapp.exec();
}
