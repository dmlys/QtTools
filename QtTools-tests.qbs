import qbs
import qbs.Environment

CppApplication
{
	type: base.concat("autotest")
	consoleApplication: true

	Depends { name: "cpp" }
	Depends { name: "extlib" }
	Depends { name: "QtTools" }
	
	Depends { name: "dmlys.qbs-common"; required: false }
	Depends { name: "ProjectSettings"; required: false }

	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

	cpp.cxxLanguageVersion : "c++17"
	// on msvc boost are usually static, on posix - shared
	cpp.defines: qbs.toolchain.contains("msvc") ? [] : ["BOOST_TEST_DYN_LINK"]

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
