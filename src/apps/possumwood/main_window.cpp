#include "main_window.h"

#include <fstream>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QHeaderView>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>

#include <bind.h>
#include <dependency_graph/attr.inl>
#include <dependency_graph/metadata.inl>
#include <dependency_graph/values.inl>
#include <dependency_graph/io/graph.h>

#include "adaptor.h"
#include "node_data.h"
#include "metadata.h"
#include "io/io.h"

namespace {

QAction* makeAction(QString title, std::function<void()> fn, QWidget* parent) {
	QAction* result = new QAction(title, parent);
	node_editor::qt_bind(result, SIGNAL(triggered(bool)), fn);
	return result;
}

}

MainWindow::MainWindow() : QMainWindow(), m_nodeCounter(0) {
	// initialise the graph with some nodes (to be removed)
	{
		auto& add = m_graph.nodes().add(Metadata::instance("addition"), "add1");
		add.setBlindData(NodeData{QPointF(-100, 20)});

		auto& mult = m_graph.nodes().add(Metadata::instance("multiplication"), "mult1");
		mult.setBlindData(NodeData{QPointF(100, 20)});

		add.port(2).connect(mult.port(0));
	}

	// create the main widget, and the main window content
	QWidget* main = new QWidget();
	setCentralWidget(main);

	QHBoxLayout* layout = new QHBoxLayout(main);
	layout->setContentsMargins(0, 0, 0, 0);

	m_viewport = new Viewport();
	layout->addWidget(m_viewport, 1);

	QVBoxLayout* rightLayout = new QVBoxLayout();
	layout->addLayout(rightLayout);
	rightLayout->setContentsMargins(0, 0, 0, 0);

	m_properties = new Properties();
	rightLayout->addWidget(m_properties);

	m_adaptor = new Adaptor(&m_graph);
	rightLayout->addWidget(m_adaptor);

	// connect the selection signal
	m_adaptor->scene().setNodeSelectionCallback(
	[&](std::set<std::reference_wrapper<node_editor::Node>, node_editor::GraphScene::NodeRefComparator> selection) {
		m_properties->show(m_adaptor->selectedNodes());
	}
	);

	// create the context click menu
	m_adaptor->setContextMenuPolicy(Qt::ActionsContextMenu);

	for(auto& m : Metadata::instances()) {
		m_adaptor->addAction(makeAction(("Add " + m.type() + " node").c_str(), [&m, this]() {
			QPointF pos = m_adaptor->mapToScene(m_adaptor->mapFromGlobal(QCursor::pos()));
			m_graph.nodes().add(m, m.type() + "_" + std::to_string(m_nodeCounter++), NodeData{pos});
		}, m_adaptor));
	}

	QAction* separator = new QAction(m_adaptor);
	separator->setSeparator(true);
	m_adaptor->addAction(separator);

	QAction* deleteAction = new QAction("Delete selected items", m_adaptor);
	deleteAction->setShortcut(QKeySequence::Delete);
	m_adaptor->addAction(deleteAction);
	node_editor::qt_bind(deleteAction, SIGNAL(triggered()), [this]() {
		m_adaptor->deleteSelected();
	});

	// drawing callback
	connect(m_viewport, SIGNAL(render(float)), this, SLOT(draw(float)));
	m_graph.onDirty([this]() {
		m_viewport->update();
	});

	////////////////////
	// window actions
	QAction* newAct = new QAction("&New...", this);
	newAct->setShortcuts(QKeySequence::New);
	connect(newAct, &QAction::triggered, [&](bool) {
		QMessageBox::StandardButton res = QMessageBox::question(this, "New file...", "Do you want to clear the scene?");
		if(res == QMessageBox::Yes) {
			m_adaptor->graph().nodes().clear();
			m_filename = "";
		}
	});

	QAction* openAct = new QAction("&Open...", this);
	openAct->setShortcuts(QKeySequence::Open);
	connect(openAct, &QAction::triggered, [this](bool) {
		QString filename = QFileDialog::getOpenFileName(this, tr("Open File"),
		                   m_filename.string().c_str(),
		                   tr("Possumwood files (*.psw)"));

		if(!filename.isEmpty()) {
			try {
				std::ifstream in(filename.toStdString());

				dependency_graph::io::json json;
				in >> json;

				dependency_graph::io::adl_serializer<dependency_graph::Graph>::from_json(json, m_adaptor->graph());

				m_filename = filename.toStdString();
			}
			catch(std::exception& err) {
				QMessageBox::critical(this, "Error loading file...", "Error loading " + filename + ":\n" + err.what());
			}
			catch(...) {
				QMessageBox::critical(this, "Error loading file...", "Error loading " + filename + ":\nUnhandled exception thrown during loading.");
			}
		}
	});

	QAction* saveAct = new QAction("&Save...", this);
	saveAct->setShortcuts(QKeySequence::Save);
	QAction* saveAsAct = new QAction("Save &As...", this);
	saveAsAct->setShortcuts(QKeySequence::SaveAs);

	connect(saveAct, &QAction::triggered, [saveAsAct, this](bool) {
		if(m_filename.empty())
			saveAsAct->triggered(true);

		else {
			try {
				std::ofstream out(m_filename.string());

				dependency_graph::io::json json;
				json = m_adaptor->graph();

				out << std::setw(4) << json;
			}
			catch(std::exception& err) {
				QMessageBox::critical(this, "Error saving file...", "Error saving " + QString(m_filename.string().c_str()) + ":\n" + err.what());
			}
			catch(...) {
				QMessageBox::critical(this, "Error saving file...", "Error saving " + QString(m_filename.string().c_str()) + ":\nUnhandled exception thrown during saving.");
			}
		}
	});

	connect(saveAsAct, &QAction::triggered, [saveAct, this](bool) {
		QString filename = QFileDialog::getSaveFileName(this, tr("Save File"),
		                   m_filename.string().c_str(),
		                   tr("Possumwood files (*.psw)"));

		if(!filename.isEmpty()) {
			m_filename = filename.toStdString();

			saveAct->triggered(true);
		}
	});


	////////////////////
	// create menu

	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(saveAct);
	fileMenu->addAction(saveAsAct);
}

void MainWindow::draw(float dt) {
	// draw the floor grid
	glBegin(GL_LINES);

	for(int a = -10; a <= 10; ++a) {
		const float c = 0.3f + (float)(a % 10 == 0) * 0.2f;
		glColor3f(c, c, c);

		glVertex3f(a, 0, -10);
		glVertex3f(a, 0, 10);
	}

	for(int a = -10; a <= 10; ++a) {
		const float c = 0.3f + (float)(a % 10 == 0) * 0.2f;
		glColor3f(c, c, c);

		glVertex3f(-10, 0, a);
		glVertex3f(10, 0, a);
	}

	glEnd();

	// draw everything else
	for(auto& n : m_adaptor->graph().nodes()) {
		glPushAttrib(GL_ALL_ATTRIB_BITS);

		const Metadata& meta = dynamic_cast<const Metadata&>(n.metadata());
		meta.draw(dependency_graph::Values(n));

		glPopAttrib();
	}

}
