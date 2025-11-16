# swine

a crappy build tool. built in c++

in terms of actually what it has cba for examples rn but: 
- it has Templates for Watch look at `SwineWatchTemplate` has
- a half-assed reflection system for build configs
- a rebuild yourself feature
- some other stuff too

to use just copy swine.h into whatever project you want create a swine.cpp or whatever you want

a small example could be i know this looks like a lot but it does a lot too!

```c++
#include "swine.h"

void GenerateFunc(int argc, char** argv) {
    BuildConfig config;

    if (!getConfig(&config)) {
        SwineLog(ERR, "failed to get the config");
        exit(1);
    }

    string ninjaFile = "build.ninja";
    ofstream file(ninjaFile);

    if (!file.is_open()) {
        SwineLog(ERR, "failed to create the ninja build file: " + ninjaFile);
        return;
    }

    file << "target=" << config.targetDir << "\n";
    file << "objdir=" << config.objDir << "\n";

    PlatformDependantBC profile = getProfile<BuildConfig, PlatformDependantBC>(config);
    vector<string> cxxFlags;
    vector<string> ldFlags = {};

    if (auto opt = arrayToStringVector(profile.cxxFlags)) {
        cxxFlags = std::move(*opt);
    } else {
        SwineLog(ERR, "cxxFlags in config are malformed");
        exit(1);
    }

    if (auto opt = arrayToStringVector(profile.ldFlags)) {
        ldFlags = std::move(*opt);
    } else {
        SwineLog(ERR, "ldFlags in config are malformed");
        exit(1);
    }

    file << "cxxflags=" << ConcatMacroList(cxxFlags) << "\n";
    file << "ldflags=" << ConcatMacroList(ldFlags) << "\n\n";
    file << "rule cxx\n";
    file << "   command = " << profile.compiler << " $cxxflags -c $in -o $out\n";
    file << "rule link\n";
    file << "   command = " << profile.compiler << " $ldflags $in -o $out\n\n";

    auto files = SwineGetFiles();

    for (const auto& module : files.modules) {
        auto opts = module.opts;

        PlatformDependantBC moduleProfile = getProfile<ModuleOptions, PlatformDependantBC>(opts);

        if (!moduleProfile.compiler.empty()) {
            vector<string> moduleCxxFlags;
            vector<string> moduleLdFlags = {};

            if (auto opt = arrayToStringVector(moduleProfile.cxxFlags)) {
                moduleCxxFlags = std::move(*opt);
            } else {
                SwineLog(ERR, "CxxFlags in config are malformed");
                exit(1);
            }

            if (auto opt = arrayToStringVector(moduleProfile.ldFlags)) {
                moduleLdFlags = std::move(*opt);
            } else {
                SwineLog(ERR, "ldFlags in config are malformed");
                exit(1);
            }

            string ruleName = fs::path(module.entryPoint).parent_path().stem().string();
            string moduleCxxFlagsVar = ruleName + "cxxflags";
            string moduleLdFlagsVar = ruleName + "ldflags";

            file << moduleCxxFlagsVar << "=" << ConcatMacroList(moduleCxxFlags) << "\n";
            file << moduleLdFlagsVar << "=" << ConcatMacroList(moduleLdFlags) << "\n\n";
            file << "rule " << ruleName << "\n";
            file << "   command = " << moduleProfile.compiler << " $" << moduleCxxFlagsVar << " $" << moduleLdFlagsVar << " $in -o $out\n\n";

            vector<string> moduleAdditionalSources;

            if (auto opt = arrayToStringVector(opts.addsources)) {
                moduleAdditionalSources = std::move(*opt);
            } else {
                SwineLog(ERR, "addsources in config are malformed");
                exit(1);
            }

            if (opts.outfolder.empty()) {
                file << "build $target/" << moduleProfile.outputName << ": " << ruleName << " " << module.entryPoint << " " <<  ConcatMacroList(moduleAdditionalSources);
            } else {
                file << "build $target/" << opts.outfolder << "/" << moduleProfile.outputName << ": " << ruleName << " " << module.entryPoint << " " << ConcatMacroList(moduleAdditionalSources);
            }

            file << "\n";
        }
    }

    vector<string> objfiles;
    for (const auto& src : files.sources) {
        string stem = fs::path(src).stem().string() + ".o";
        objfiles.push_back(stem);
        file << "build $objdir/" << stem << ": cxx " << src << "\n";
    }

    file << "\nbuild $target/" << profile.outputName << ": link";
    for (const auto& obj: objfiles) {
        file << " $objdir/" << obj;
    }
    file << "\n";

    file.close();
    SwineLog(INFO, "ninja build script generation finished outputted to -> " + ninjaFile);

    // for your language server (clangd) or whatver
    BuildCompileCommands();
}

SwineWatchTemplate(WatchFunc, 
    GenerateFunc(argc, argv);
    Task ninja = {{"ninja"}};
    ninja.run();
)

void CleanFunc(int argc, char** argv) {
    BuildConfig config;

    if (!getConfig(&config)) {
        SwineLog(ERR, "failed to get the config");
        exit(1);
    }

    SwineLog(INFO, "cleaning target directory -> '" + string(config.targetDir) + "'") ;
    try {
        if (fs::exists(config.targetDir) && fs::is_directory(config.targetDir)) {
            for (const auto& entry : fs::directory_iterator(config.targetDir)) {
                if (fs::is_directory(entry.path())) {
                    fs::remove_all(entry.path());
                } else {
                    fs::remove(entry.path());
                }
            }
            fs::remove_all(config.targetDir);
            fs::remove("compile_commands.json");
            fs::remove("build.ninja");
            fs::remove(".ninja_log");
            SwineLog(INFO, "successfully cleaned target directory");
        } else {
            SwineLog(ERR, "target directory either doesn't exist or is a file");
        }
    } catch (const fs::filesystem_error& e) {
        SwineLog(ERR, "error while cleaning target directory: " + string(e.what()));
    }
}

SwineBuildMain(
    app.cmds.push_back({"gen", "generate a ninja build script", GenerateFunc});
    app.cmds.push_back({"watch", "watch over files in src dir", WatchFunc});
    app.cmds.push_back({"clean", "cleans the target directory", CleanFunc});
)

```

taken inspiration from -> https://github.com/tsoding/nob.h 

feel free to rip parts out... mutalitate my code reuse my code

;) - s.c
