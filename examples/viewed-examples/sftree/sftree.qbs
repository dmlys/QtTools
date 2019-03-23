import qbs
import qbs.Environment

CppApplication
{
	Depends { name: "cpp" }
	Depends { name: "extlib" }
	Depends { name: "QtTools" }

	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

	cpp.cxxLanguageVersion : "c++17"
	cpp.cxxFlags: project.additionalCxxFlags
	cpp.driverFlags: project.additionalDriverFlags
	//cpp.defines: project.additionalDefines
	cpp.systemIncludePaths: project.additionalSystemIncludePaths
	cpp.includePaths: ["include"].uniqueConcat(project.additionalIncludePaths || [])
	cpp.libraryPaths: project.additionalLibraryPaths

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
