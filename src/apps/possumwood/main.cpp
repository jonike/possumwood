#include <iostream>
#include <string>
#include <stdexcept>

#include <dlfcn.h>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QHBoxLayout>

#include <dependency_graph/graph.h>
#include <dependency_graph/node.inl>
#include <dependency_graph/datablock.inl>
#include <dependency_graph/metadata.inl>
#include <dependency_graph/attr.inl>

#include <qt_node_editor/node.h>
#include <qt_node_editor/connected_edge.h>
#include <qt_node_editor/graph_widget.h>

#include <possumwood_sdk/app.h>

#include "adaptor.h"
#include "main_window.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using std::cout;
using std::endl;
using std::flush;

#ifndef PLUGIN_DIR
#define PLUGIN_DIR "./plugins"
#endif

int main(int argc, char* argv[]) {
	// // Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()
	("help", "produce help message")
	("plugin_directory", po::value<std::string>()->default_value(PLUGIN_DIR), "directory to search for plugins")
	("scene", po::value<std::string>(), "open a scene file")
	;

	// process the options
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if(vm.count("help")) {
		cout << desc << "\n";
		return 1;
	}

	///////////////////////////////

	std::vector<void*> pluginHandles;

	// scan for plugins
	for(fs::directory_iterator itr(vm["plugin_directory"].as<std::string>()); itr != fs::directory_iterator(); ++itr) {
		if(fs::is_regular_file(itr->status()) && itr->path().extension() == ".so") {
			void* ptr = dlopen(itr->path().string().c_str(), RTLD_NOW);
			if(ptr)
				pluginHandles.push_back(ptr);
			else
				std::cout << dlerror() << std::endl;
		}
	}

	///////////////////////////////

	{
		// create the application object
		QApplication app(argc, argv);

		// initialise glut
		glutInit(&argc, argv);

		// create the possumwood application
		possumwood::App papp;

		// make a main window
		MainWindow win;
		win.setWindowIcon(QIcon(":icons/app.png"));
		win.showMaximized();

		// initialize GLEW
		auto glewErr = glewInit();
		if(glewErr != GLEW_OK) {
			std::cout << "Error initialising GLEW - " << glewGetErrorString(glewErr) << std::endl;
			exit(1);
		}

		// open the scene file, if specified on the command line
		if(vm.count("scene"))
			possumwood::App::instance().loadFile(vm["scene"].as<std::string>());

		// and start the main application loop
		app.exec();
	}

	////////////////////////////////

	// unload all plugins
	while(!pluginHandles.empty()) {
		dlclose(pluginHandles.back());
		pluginHandles.pop_back();
	}

	return 0;
}
