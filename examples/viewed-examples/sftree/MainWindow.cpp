#include "MainWindow.hqt"
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

QSize MainWindow::sizeHint() const
{
	// maximum size - half screen
	QSize maxSize = QApplication::desktop()->screenGeometry().size();
	maxSize /= 2;

	return maxSize;
}

auto MainWindow::GenerateAssignData() const -> std::vector<test_tree_entity>
{
	std::vector<test_tree_entity> data =
	{
	    {"folder/file1.txt",          "text-descr1", 1},
	    {"folder/file2.txt",          "text-descr2", 2},
	    {"folder/file3.txt",          "text-descr3", 3},
	    {"dir/file1.sft",             "text-descr4", 4},
	    {"dir/prox/dir.txt",          "text-descr5", 5},
	    {"ops.sh",                    "text-descr6", 6},
	    {"westworld.mkv",             "text-descr7", 7},
	    {"folder/sup/file3.txt",      "text-descr8", 8},
	    {"folder/sup/inner/file.txt", "text-descr9", 9},
	};

	return data;
}

auto MainWindow::GenerateUpsertData() const -> std::vector<test_tree_entity>
{
	std::vector<test_tree_entity> data =
	{
	    {"dir/file1.sft",            "updated-text-descr4", 44},
	    {"dir/prox/dir.txt",         "updated-text-descr5", 55},

	    {"upsershalt/ziggaman.txt",  "new-text-1", 10},
	    {"summer-bucket",            "new-text-2", 11},
	};

	return data;
}

void MainWindow::AssignData(TestTreeModel & model)
{
	auto data = GenerateAssignData();
	model.assign(data | ext::moved);
}

void MainWindow::AssignData(TestTreeViewModel & model)
{
	auto data = GenerateAssignData();
	model.get_owner()->assign(data | ext::moved);
}

void MainWindow::UpsertData(TestTreeModel & model)
{
	auto data = GenerateUpsertData();
	model.upsert(data | ext::moved);
}

void MainWindow::UpsertData(TestTreeViewModel & model)
{
	auto data = GenerateUpsertData();
	model.get_owner()->upsert(data | ext::moved);
}

void MainWindow::ClearData(TestTreeModel & model)
{
	model.clear();
}

void MainWindow::ClearData(TestTreeViewModel & model)
{
	model.get_owner()->clear();
}

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
{
	setupUi();
	connectSignals();
	setupModels();
}

void MainWindow::setupModels()
{
	m_container = std::make_shared<TestTreeContainer>();
	m_model = std::make_shared<TestTreeModel>();

	m_viewModel1 = std::make_shared<TestTreeViewModel>(m_container);
	m_viewModel2 = std::make_shared<TestTreeViewModel>(m_container);

	m_view1->SetModel(m_model.get());
	m_view2->SetModel(m_viewModel1.get());
	m_view3->SetModel(m_viewModel2.get());

	m_view1->Sort(0, Qt::AscendingOrder);
	m_view2->Sort(0, Qt::AscendingOrder);
	m_view3->Sort(0, Qt::DescendingOrder);
}

static QVBoxLayout * createViewLayout(TestTreeView * view, QPushButton * assign, QPushButton * upsert, QPushButton * clear)
{
	auto * buttonsLayout = new QHBoxLayout;
	buttonsLayout->addWidget(assign);
	buttonsLayout->addWidget(upsert);
	buttonsLayout->addWidget(clear);

	auto * viewLayout = new QVBoxLayout;
	viewLayout->addWidget(view);
	viewLayout->addLayout(buttonsLayout);

	return viewLayout;
}

void MainWindow::setupUi()
{	
	m_assign1 = new QPushButton(tr("assign data"));
	m_assign2 = new QPushButton(tr("assign data"));
	m_assign3 = new QPushButton(tr("assign data"));

	m_upsert1 = new QPushButton(tr("upsert data"));
	m_upsert2 = new QPushButton(tr("upsert data"));
	m_upsert3 = new QPushButton(tr("upsert data"));

	m_clear1 = new QPushButton(tr("clear data"));
	m_clear2 = new QPushButton(tr("clear data"));
	m_clear3 = new QPushButton(tr("clear data"));

	m_view1 = new TestTreeView;
	m_view2 = new TestTreeView;
	m_view3 = new TestTreeView;

	auto * splitter = new QSplitter;
	auto * splitter2 = new QSplitter;

	auto * viewLayout = createViewLayout(m_view1, m_assign1, m_upsert1, m_clear1);
	auto * group = new QGroupBox("separate model");
	group->setLayout(viewLayout);
	splitter->addWidget(group);

	QWidget * helper;
	viewLayout = createViewLayout(m_view2, m_assign2, m_upsert2, m_clear2);
	helper = new QWidget;
	helper->setLayout(viewLayout);
	splitter2->addWidget(helper);

	viewLayout = createViewLayout(m_view3, m_assign3, m_upsert3, m_clear3);
	helper = new QWidget;
	helper->setLayout(viewLayout);
	splitter2->addWidget(helper);

	group = new QGroupBox(tr("shared model"));
	auto * layout = new QVBoxLayout(group);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(splitter2);

	splitter->addWidget(group);

	setCentralWidget(splitter);
}

void MainWindow::connectSignals()
{
	connect(m_assign1, &QPushButton::clicked, this, [this] { AssignData(*m_model); });
	connect(m_assign2, &QPushButton::clicked, this, [this] { AssignData(*m_viewModel1); });
	connect(m_assign3, &QPushButton::clicked, this, [this] { AssignData(*m_viewModel2); });

	connect(m_upsert1, &QPushButton::clicked, this, [this] { UpsertData(*m_model); });
	connect(m_upsert2, &QPushButton::clicked, this, [this] { UpsertData(*m_viewModel1); });
	connect(m_upsert3, &QPushButton::clicked, this, [this] { UpsertData(*m_viewModel2); });

	connect(m_clear1, &QPushButton::clicked, this, [this] { ClearData(*m_model); });
	connect(m_clear2, &QPushButton::clicked, this, [this] { ClearData(*m_viewModel1); });
	connect(m_clear3, &QPushButton::clicked, this, [this] { ClearData(*m_viewModel2); });
}
