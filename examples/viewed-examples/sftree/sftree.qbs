import qbs
import qbs.Environment

CppApplication
{
	Depends { name: "cpp" }
	Depends { name: "extlib" }
	Depends { name: "QtTools" }

	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

	Depends { name: "dmlys.qbs-common"; required: false }
	Depends { name: "ProjectSettings"; required: false }

	cpp.cxxLanguageVersion : "c++17"
	cpp.includePaths: ["include"]
	cpp.dynamicLibraries: ["stdc++fs", "boost_system", "fmt"]


	FileTagger {
		patterns: "*.hqt"
		fileTags: ["hpp"]
	}

	files: [
        "AbstractTestModel.cpp",
        "AbstractTestModel.hqt",
        "MainWindow.cpp",
        "MainWindow.hqt",
        "TestTreeEntity.hpp",
        "TestTreeEntityContainer.hpp",
        "TestTreeModel.cpp",
        "TestTreeModel.hqt",
        "TestTreeView.hqt",
        "TestTreeView.cpp",
        "main.cpp",
    ]
}
