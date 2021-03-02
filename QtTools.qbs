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
	cpp.cxxFlags: project.additionalCxxFlags
	cpp.driverFlags: project.additionalDriverFlags
	//cpp.defines: project.additionalDefines
	cpp.systemIncludePaths: project.additionalSystemIncludePaths
	cpp.includePaths: ["include"].uniqueConcat(project.additionalIncludePaths || [])
	cpp.libraryPaths: project.additionalLibraryPaths


	Export
	{
		Depends { name: "cpp" }

		cpp.cxxLanguageVersion : "c++17"
		cpp.includePaths : ["include"]
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
