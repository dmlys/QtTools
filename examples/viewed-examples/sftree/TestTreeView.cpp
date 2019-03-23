#include "TestTreeView.hqt"

#include <QtGui/QClipboard>
#include <QtWidgets/QShortcut>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <QtTools/ToolsBase.hpp>
#include <QtTools/ItemViewUtils.hpp>

#include <QtTools/ItemViewUtils.hpp>
#include <QtTools/HeaderConfigurationWidget.hqt>


void TestTreeView::OnFilterChanged()
{
	SetFilter(m_rowFilter->text());
}

void TestTreeView::ModelChanged()
{
	ResizeColumnsToContents();
	updateGeometry();
}

void TestTreeView::SetFilter(QString newFilter)
{
	m_filterString = std::move(newFilter);
	m_nameDelegate->SetFilterText(m_filterString);

	if (m_model)
	{
		m_model->SetFilter(m_filterString);
		m_treeView->viewport()->update();
	}
}

void TestTreeView::Sort(int column, Qt::SortOrder order)
{
	m_sortColumn = column;
	m_sortOrder = order;

	m_treeView->sortByColumn(m_sortColumn, m_sortOrder);
}

/************************************************************************/
/*                Context Menu                                          */
/************************************************************************/
void TestTreeView::OpenHeaderConfigurationWidget()
{
	if (not m_headerModel)
	{
		m_headerModel = new QtTools::HeaderControlModel(this);
		m_headerModel->Track(m_treeView->header());

		if (m_headerConfig)
			m_headerModel->Configurate(*m_headerConfig);
	}


	auto * confWgt = findChild<QtTools::HeaderConfigurationWidget *>("ConfigurationWidget");
	if (confWgt)
	{
		confWgt->show();
		confWgt->activateWindow();
		return;
	}

	confWgt = new QtTools::HeaderConfigurationWidget(*m_headerModel, this);
	confWgt->setObjectName("ConfigurationWidget");
	confWgt->show();
	confWgt->activateWindow();
}

void TestTreeView::ViewSettings()
{
	OpenHeaderConfigurationWidget();
}

void TestTreeView::ResizeColumnsToContents()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);

	// what we need is done by resizeColumnsToContents,
	// but it's also takes into account headers, and i want without them - have to do by hand
	m_treeView->resizeColumnToContents(0);

	auto * header = m_treeView->header();
	auto minimum = header->minimumSectionSize();
	int count = header->count();

	for (int i = 0; i < count; ++i)
	{
		auto li = header->logicalIndex(i);
		if (not header->isSectionHidden(i))
		{
			// for some unknown reason virtual method sizeHintForColumn is declared as protected in QTableView,
			// through it's public QAbstractItemView. call through base class
			auto hint = static_cast<QAbstractItemView *>(m_treeView)->sizeHintForColumn(li);
			header->resizeSection(li, std::max(hint, minimum));
		}
	}

	QApplication::restoreOverrideCursor();
}

QSize TestTreeView::sizeHint() const
{
	return QtTools::ItemViewSizeHint(this, m_treeView);
}

/************************************************************************/
/*                    Init Methods                                      */
/************************************************************************/
void TestTreeView::ConnectModel()
{
	auto * model = m_model;
	m_treeView->setModel(model);
	m_rowFilter->clear();

	auto name_col = model->MetaToViewIndex(0);
	m_treeView->setItemDelegateForColumn(name_col, m_nameDelegate);

	connect(model, &QAbstractItemModel::layoutChanged, this, &TestTreeView::ModelChanged);
	connect(model, &QAbstractItemModel::modelReset, this, &TestTreeView::ModelChanged);
	connect(model, &AbstractTestModel::SortingChanged, this, &TestTreeView::SortingChanged);

	connect(m_treeView->header(), &QHeaderView::customContextMenuRequested,
			this, &TestTreeView::OpenHeaderConfigurationWidget);

	if (m_sortColumn >= 0) m_treeView->sortByColumn(m_sortColumn, m_sortOrder);
}

void TestTreeView::DisconnectModel()
{
	delete m_headerModel;
	m_headerModel = nullptr;
	m_treeView->setModel(nullptr);
}

void TestTreeView::SetModel(AbstractTestModel * model)
{
	// both null or valid
	if (m_model) m_model->disconnect(this);
	m_model = std::move(model);

	if (m_model)
	{
		ConnectModel();
		updateGeometry();
	}
	else
	{
		DisconnectModel();
	}
}

void TestTreeView::InitHeaderTracking(QtTools::HeaderSectionInfoList * headerConf /* = nullptr */)
{
	m_headerConfig = headerConf;
	if (not m_headerModel)
	{
		m_headerModel = new QtTools::HeaderControlModel(this);
		m_headerModel->Track(m_treeView->header());
	}

	if (m_headerConfig)
		m_headerModel->Configurate(*m_headerConfig);
}

TestTreeView::TestTreeView(QWidget * parent /* = nullptr */) : QFrame(parent)
{
	setupUi();
	connectSignals();
	retranslateUi();
}

TestTreeView::~TestTreeView()
{
	if (m_headerConfig)
		*m_headerConfig = m_headerModel->SaveConfiguration();
}

void TestTreeView::connectSignals()
{
	//: filter shortcut in generic QTableView/QListView, shown as placeholder in QLineEdit
	auto * filterShortcut = new QShortcut(QKeySequence(tr("Ctrl+F")), this);
	connect(filterShortcut, &QShortcut::activated,
			m_rowFilter, static_cast<void (QLineEdit::*)()>(&QLineEdit::setFocus));

	connect(m_rowFilter, &QLineEdit::textChanged, this, &TestTreeView::OnFilterChanged);
}

void TestTreeView::setupUi()
{
	m_verticalLayout = new QVBoxLayout(this);
	m_rowFilter = new QLineEdit(this);
	m_rowFilter->setClearButtonEnabled(true);

	m_treeView = new QTreeView(this);
	m_treeView->setSortingEnabled(true);
	m_treeView->setAlternatingRowColors(true);
	m_treeView->setUniformRowHeights(true);
	m_treeView->setAnimated(true);
	m_treeView->setTabKeyNavigation(false);

	auto * vertHeader = m_treeView->header();
	//vertHeader->setDefaultSectionSize(QtTools::CalculateDefaultRowHeight(m_treeView));
	vertHeader->setDefaultSectionSize(21);

	vertHeader->setSortIndicator(-1, Qt::AscendingOrder);
	//vertHeader->setSectionsMovable(true);
	vertHeader->setContextMenuPolicy(Qt::CustomContextMenu);
	vertHeader->setDefaultAlignment(Qt::AlignLeft);
	//vertHeader->setTextElideMode(Qt::ElideRight);

	m_treeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_nameDelegate = new QtTools::Delegates::SearchDelegate(this);

	m_verticalLayout->addWidget(m_rowFilter);
	m_verticalLayout->addWidget(m_treeView);
}

void TestTreeView::retranslateUi()
{
	//: filter shortcut in generic QTableView/QListView, shown as placeholder in QLineEdit
	m_rowFilter->setPlaceholderText(tr("Row filter(Ctrl+F)"));
}
