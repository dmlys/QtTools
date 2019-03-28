import qbs
import qbs.Environment

CppApplication
{
	type: base.concat("autotest")

	Depends { name: "cpp" }
	Depends { name: "extlib" }
	Depends { name: "QtTools" }

	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

	cpp.cxxLanguageVersion : "c++17"
	cpp.cxxFlags: project.additionalCxxFlags
	cpp.driverFlags: project.additionalDriverFlags
	cpp.defines: ["BOOST_TEST_DYN_LINK"].uniqueConcat(project.additionalDefines || [])
	cpp.systemIncludePaths: project.additionalSystemIncludePaths
	cpp.includePaths: ["include"].uniqueConcat(project.additionalIncludePaths || [])
	cpp.libraryPaths: project.additionalLibraryPaths

	cpp.dynamicLibraries: [
		"stdc++fs", "fmt",
		//"boost_system",
		//"boost_test_exec_monitor",
		"boost_unit_test_framework"
	]

	FileTagger {
		patterns: "*.hqt"
		fileTags: ["hpp"]
	}

	files: [
		"tests/**"
    ]
}
