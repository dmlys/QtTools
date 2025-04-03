import qbs
import qbs.Environment

StaticLibrary
{
	Depends { name: "cpp" }
	Depends { name: "extlib" }
	Depends { name: "dmlys.qbs-common"; required: false }
	Depends { name: "ProjectSettings"; required: false }

	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

	cpp.cxxLanguageVersion : "c++17"
	cpp.includePaths: ["include"]

	
	Export
	{
		Depends { name: "cpp" }

		cpp.cxxLanguageVersion : "c++17"
		cpp.includePaths : [exportingProduct.sourceDirectory + "/include"]
	}

	FileTagger {
		patterns: "*.hqt"
		fileTags: ["hpp"]
	}

	files: [
		"include/**",
		"src/**",
		"resources/**",
	]
}
