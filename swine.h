// @calizoots on github

#ifndef SWINE_H
#define SWINE_H
#include <cstddef>
#include <iterator>
#include <string>
#include <cstring>
#include <algorithm>
#include <optional>
#include <iostream>
#include <filesystem>
#include <variant>
#include <thread>
#include <vector>
#include <fstream>
#include <map>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define SwineLogDebug 1
#define ConfigFile ".build"

#if _WIN32
    #define PLATFORM windows
#elif __APPLE__
    #define PLATFORM mac
#else
    #define PLATFORM linux
#endif

using std::string;
using std::cout;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::map;
using std::optional;
using std::cerr;
using std::runtime_error;
using std::variant;
using std::to_string;
using std::string_view;
using std::istreambuf_iterator;
using std::holds_alternative;
using std::nullptr_t;
using std::endl;
using std::is_same_v;

namespace fs = std::filesystem;

static inline string ConcatMacroList(const vector<string>& list) {
    string result;
    for (const auto& part : list) {
        result += part + " ";
    }
    return result;
}

enum LogLvl {
    INFO,
    WARN,
    ERR
};

using LogLvl::INFO;
using LogLvl::WARN;
using LogLvl::ERR;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"
#define YELLOW  "\033[33m"
#define ORANGE "\033[38;5;208m"

#define _SwineLogStream(lvl) ((lvl) == ERR ? cerr : cout)

/// Print message to the console.
/// @param lvl Log Level
/// @param message Message can be formatted with arithmatic
#define SwineLog(lvl, msg) do {                                \
    const char* _prefix = (lvl == INFO ? "[INFO] "             \
                           : lvl == WARN ? "[WARNING] "        \
                           : "[ERROR] ");                      \
    const char* _color = (lvl == INFO ? CYAN                   \
                          : lvl == WARN ? YELLOW               \
                          : RED);                              \
    _SwineLogStream(lvl) << _color << _prefix << RESET << msg << endl; \
} while (0)

/// Debug log message to the console only prints if debug level is enough.
/// @param req minimum debug level to print
/// @param lvl Log Level
/// @param message Message can be formatted with arithmatic
#define SwineDebugLog(req, msg) do { \
    if ((req) <= SwineLogDebug) { \
        cout << ORANGE << "[DEBUG] " << RESET << msg << endl; \
    } \
} while (0)

inline int DoesExistAndIsDir(const string& path) {
    if (!fs::exists(path)) {
        return 0;
    } 

    if (fs::is_directory(path)) {
        return 1;
    }

    return -1;
}

inline bool startsWith(const string& str, const string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

#define CheckDirCreateIfNot(dir) { \
    int res = DoesExistAndIsDir(dir); \
    if (res == 0) { \
        SwineLog(INFO, "directory: '" + string(dir) + "' doesn't exist so creating..."); \
        fs::create_directories(dir); \
    } else if (res == 1) { \
        SwineDebugLog(2, "directory: '" + string(dir) + "' already exists..."); \
    } else { \
        SwineLog(ERR, "directory: '" + string(dir) + "' is a file"); \
        exit(69); \
    } \
}

struct SwineCliCmd {
    string name;
    string description;
    void (*cb)(int, char**);
};

/// Basic cli for this build system
struct SwineCli {
    vector<SwineCliCmd> cmds; 

    void help() {
        cout << "help: ./dev" << endl;
        for (const auto& cmd : cmds) {
            cout << "\t" << cmd.name << " - " << cmd.description << endl;
        }
        cout << "\t" << "help - prints this message" << endl;
    }

    void go(int argc, char** argv) {
        if (argc <= 1) {
            SwineLog(ERR, "no command provided");
            help();
            exit(69);
        }

        auto it = find_if(cmds.begin(), cmds.end(), [&](const SwineCliCmd& cmd) {
                return cmd.name == argv[1];
                });

        if (it != cmds.end()) {
            it->cb(argc, argv);
        } else if (string(argv[1]) == "help") {
            help();
        } else {
            SwineLog(ERR, "unknown command: " + string(argv[1]));
            help();
        }
    }
};

/// Basic cross platform way to run a shell command
struct Task {
    /// list of arguement split on spaces
    vector<string> cmd;

    string output = "";

    /// Run the command
    /// @param log Logger
    /// @param saveToFile useless rn
    int run(bool saveToOutput = false) {
        if (cmd.empty()) {
            SwineLog(ERR, "task failed command is empty");
            return -1;
        }

        string fullCmd;
        for (const auto& part : cmd) {
            fullCmd += part + " ";
        }
        
        if (!fullCmd.empty()) {
            fullCmd.pop_back();
        }

        SwineLog(INFO, "starting task '"+ fullCmd + "'");

        #ifdef _WIN32
            STARTUPINFO si = { sizeof(STARTUPINFO) };
            PROCESS_INFORMATION pi;
        
            string commandStr = fullCmd;
            char* command = &commandStr[0];
        
            if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                LogDebug(ERR, "failed to start process: " + fullCmd);
                return -1;
            }
        
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return exitCode;
        #else
            int pipefd[2];
            if (saveToOutput) {
                if (pipe(pipefd) == -1) {
                    SwineLog(ERR, "failed to create pipe");
                    return -1;
                }
            }
            pid_t pid = fork();
        
            if (pid == -1) {
                SwineLog(ERR, "failed to fork process for task");
                return -1;
            } else if (pid == 0) {
                if (saveToOutput) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    dup2(pipefd[1], STDERR_FILENO);
                    close(pipefd[1]);
                }

                vector<char*> args;
                for (auto& arg : cmd) args.push_back(strdup(arg.c_str()));
                args.push_back(nullptr);
        
                execvp(args[0], args.data());
                perror("execvp failed");
                exit(1);
            } else {
                if (saveToOutput) {
                    close(pipefd[1]);
                    char buffer[512];
                    ssize_t bytesRead;

                    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[bytesRead] = '\0';
                        output += buffer;
                    }

                    close(pipefd[0]);
                }

                int status;
                waitpid(pid, &status, 0);
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
        #endif
    }
};

#ifndef rebuild_yourself
    #if _win32
        #if defined(__gnuc__)
            #define rebuild_yourself(binary_path, source_path) "g++", "-std=c++17", "-o", binary_path, source_path
        #elif defined(__clang__)
            #define REBUILD_YOURSELF(binary_path, source_path) "clang++","-std=c++17", "-o", binary_path, source_path
        #endif
    #else
        #define REBUILD_YOURSELF(binary_path, source_path) "c++", "-std=c++17", "-o", binary_path, source_path
    #endif
#endif

#define RebuildStatFile(path, nstat) \
    if (stat(path, nstat) != 0) { \
        SwineLog(ERR, "failed to stat binary '" + string(path) + "'"); \
        return; \
    }

static inline bool HasEnding(string const &fullString, string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

/// Go ands rebuild itself works on something stolen off someone else
static inline void GoRebuildYourself(int argc, char** argv) {
    if (argc < 1 && !argv[0]) {
        SwineLog(ERR, "invalid binary path");
    }

    const char* binaryPath = argv[0]; 
    const char* sourcePath = __FILE__;

    string headerFile(sourcePath);
    string cppPath;

    if (HasEnding(headerFile, ".h")) {
        cppPath = headerFile.substr(0, headerFile.size() - 2) + ".cpp";
    } else {
        SwineLog(ERR, "could not infer .cpp file from header: " + headerFile);
        return;
    }

    struct stat binaryStat, sourceStat, cppStat;

    RebuildStatFile(binaryPath, &binaryStat);
    RebuildStatFile(sourcePath, &sourceStat);
    RebuildStatFile(cppPath.c_str(), &cppStat);

    if (sourceStat.st_mtime > binaryStat.st_mtime || cppStat.st_mtime > binaryStat.st_mtime) {
        SwineDebugLog(1, "the binary is outdated it needs rebuilding");
        string oldBinaryPath = "" + string(binaryPath) + ".old";

        fs::rename(binaryPath, oldBinaryPath);

        Task rebuild = {{REBUILD_YOURSELF(binaryPath, cppPath.c_str())}};
        int buildRes = rebuild.run();

        if (buildRes != 0) {
            fs::rename(oldBinaryPath, binaryPath);
            exit(69);
        } else {
            fs::remove(oldBinaryPath);
        }

        vector<string> args(argv, argv + argc);
        Task run = {args};
        run.run(); 

        exit(0);

    } else {
        SwineDebugLog(1, "the binary is up to date");
    }
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct Value;

using Array = vector<Value>;
using Object = map<string, Value>;

struct Value {
    using Variant = variant<Array, Object, double, string, nullptr_t>;
    Variant data;

    Value() : data(nullptr) {}
    Value(double d) : data(d) {}
    Value(const string& s) : data(s) {}
    Value(const char* s) : data(string(s)) {}
    Value(const Array& a) : data(a) {}
    Value(const Object& o) : data(o) {}

    bool isNull()   const { return holds_alternative<nullptr_t>(data); }
    bool isNumber() const { return holds_alternative<double>(data); }
    bool isString() const { return holds_alternative<string>(data); }
    bool isArray()  const { return holds_alternative<Array>(data); }
    bool isObject() const { return holds_alternative<Object>(data); }

    double& asNumber() {
        if (!isNumber()) throw runtime_error("value is not a number");
        return std::get<double>(data);
    }
    const double& asNumber() const {
        if (!isNumber()) throw runtime_error("value is not a number");
        return std::get<double>(data);
    }

    string& asString() {
        if (!isString()) throw runtime_error("value is not a string");
        return std::get<string>(data);
    }
    const string& asString() const {
        if (!isString()) throw runtime_error("value is not a string");
        return std::get<string>(data);
    }

    Array& asArray() {
        if (!isArray()) throw runtime_error("value is not an array");
        return std::get<Array>(data);
    }
    const Array& asArray() const {
        if (!isArray()) throw runtime_error("value is not an array");
        return std::get<Array>(data);
    }

    Object& asObject() {
        if (!isObject()) throw runtime_error("value is not an object");
        return std::get<Object>(data);
    }
    const Object& asObject() const {
        if (!isObject()) throw runtime_error("value is not an object");
        return std::get<Object>(data);
    }

    Value& operator[](const string& key) {
        if (!isObject()) throw runtime_error("not an object");
        return std::get<Object>(data)[key];
    }

    const Value& operator[](const string& key) const {
        if (!isObject()) throw runtime_error("not an object");
        const auto& obj = std::get<Object>(data);
        auto it = obj.find(key);
        if (it == obj.end()) throw runtime_error("key not found: " + key);
        return it->second;
    }

    Value& operator[](size_t idx) {
        if (!isArray()) throw runtime_error("Not an array");
        auto& arr = std::get<Array>(data);
        if (idx >= arr.size()) throw runtime_error("index out of range");
        return arr[idx];
    }

    const Value& operator[](size_t idx) const {
        if (!isArray()) throw runtime_error("Not an array");
        const auto& arr = std::get<Array>(data);
        if (idx >= arr.size()) throw runtime_error("index out of range");
        return arr[idx];
    }

    bool hasKey(const string& key) const {
        if (!isObject()) return false;
        const auto& obj = std::get<Object>(data);
        return obj.find(key) != obj.end();
    }

    size_t size() const {
        if (isArray()) return std::get<Array>(data).size();
        if (isObject()) return std::get<Object>(data).size();
        throw runtime_error("not an array or object");
    }
};

static inline string lngEscape(const string& str) {
    string out = "\"";
    for (char c : str) {
        if (c == '\"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

string serialize(const Value& val);

static inline string serialize(const Array& arr) {
    string out = "[";
    bool first = true;
    for (const auto& v : arr) {
        if (!first) out += ", ";
        out += serialize(v);
        first = false;
    }
    out += "]";
    return out;
}

static inline string serialize(const Object& obj) {
    string out = "{";
    bool first = true;
    for (const auto& [key, val] : obj) {
        if (!first) out += ", ";
        out += lngEscape(key) + ": " + serialize(val);
        first = false;
    }
    out += "}";
    return out;
}

inline string serialize(const Value& val) {
    return visit(overloaded{
        [](nullptr_t) { return string("null"); },
        [](double d) { return to_string(d); },
        [](const string& s) { return lngEscape(s); },
        [](const Array& a) { return serialize(a); },
        [](const Object& o) { return serialize(o); }
    }, val.data);
}

inline string prettySerialize(const Value& val, int indent = 0) {
    const string indentStr(indent * 4, ' ');
    const string indentStrNext((indent + 1) * 4, ' ');

    if (val.isNull()) return "null";

    if (val.isNumber()) {
        return to_string(val.asNumber());
    }

    if (val.isString()) {
        return lngEscape(val.asString());
    }

    if (val.isArray()) {
        const auto& arr = val.asArray();
        if (arr.empty()) return "[]";

        string out = "[\n";
        for (size_t i = 0; i < arr.size(); ++i) {
            out += indentStrNext + prettySerialize(arr[i], indent + 1);
            if (i + 1 < arr.size()) out += ",";
            out += "\n";
        }
        out += indentStr + "]";
        return out;
    }

    if (val.isObject()) {
        const auto& obj = val.asObject();
        if (obj.empty()) return "{}";

        string out = "{\n";
        size_t count = 0;
        for (const auto& [key, v] : obj) {
            out += indentStrNext + lngEscape(key) + ": " + prettySerialize(v, indent + 1);
            if (++count < obj.size()) out += ",";
            out += "\n";
        }
        out += indentStr + "}";
        return out;
    }

    throw runtime_error("Unknown Value type");
}

struct Parser {
    string_view input;
    size_t pos = 0;

    char peek() const { return pos < input.size() ? input[pos] : '\0'; }
    void skip() { while (isspace(peek())) ++pos; }
    void expect(char c) { skip(); if (peek() != c) throw runtime_error("expected char"); ++pos; }

    string parseString() {
        expect('"');
        string out;
        while (true) {
            char c = peek();
            if (c == '\0') throw runtime_error("unexpected EOF in string");
            if (c == '"') break;
            if (c == '\\') {
                ++pos;
                char esc = peek();
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default:
                        throw runtime_error(string("invalid escape sequence: \\") + esc);
                }
                ++pos;
            } else {
                out += c;
                ++pos;
            }
        }
        expect('"');
        return out;
    }

    double parseNumber() {
        skip();
        size_t start = pos;
        while (isdigit(peek()) || peek() == '.' || peek() == '-' || peek() == '+') ++pos;
        if (start == pos) throw runtime_error("expected number");
        double val = stod(string(input.substr(start, pos - start)));
        return val;
    }

    Value parseValue() {
        skip();
        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        return parseNumber();
    }

    Array parseArray() {
        expect('[');
        Array arr;
        skip();
        if (peek() == ']') { ++pos; return arr; }
        while (true) {
            arr.push_back(parseValue());
            skip();
            if (peek() == ']') break;
            expect(',');
        }
        expect(']');
        return arr;
    }

    Object parseObject() {
        expect('{');
        Object obj;
        skip();
        if (peek() == '}') { ++pos; return obj; }
        while (true) {
            string key = parseString();
            skip();
            expect(':');
            Value val = parseValue();
            obj[key] = val;
            skip();
            if (peek() == '}') break;
            expect(',');
        }
        expect('}');
        return obj;
    }

    Value parse(string_view input_) {
        input = input_;
        pos = 0;
        return parseValue();
    }
};

#define REFLECT_FIELD(field) f(#field, field);

#define DECLARE_FIELD(type, name) type name;
#define DECLARE_FIELD_NAME(type, name) #name,
#define DECLARE_FIELD_REFLECT(type, name) \
    f(#name, name); \

#define REFLECT(...) \
    template<typename F> void reflect(F&& f) { reflectImpl(std::forward<F>(f)); } \
    template<typename F> void reflect(F&& f) const { reflectImpl(std::forward<F>(f)); } \
private: \
    template<typename F> void reflectImpl(F&& f) { __VA_ARGS__ } \
    template<typename F> void reflectImpl(F&& f) const { __VA_ARGS__ }

#define ReflectStruct(name, FIELDS) \
    struct name { \
        FIELDS(DECLARE_FIELD) \
        REFLECT(FIELDS(DECLARE_FIELD_REFLECT)); \
        static constexpr const char* fieldNames[] = { FIELDS(DECLARE_FIELD_NAME) }; \
    };

template<typename T>
T fromValue(const Value& val) {
    if (!val.isObject()) throw runtime_error("expected object");
    T obj;
    obj.reflect([&](const char* name, auto& field) {
        if (!val.hasKey(name))
            throw runtime_error(string("missing key: ") + name);

        const Value& v = val[name];
        using FieldType = std::decay_t<decltype(field)>;

        if constexpr(is_same_v<FieldType, string>) {
            if (!v.isString()) throw runtime_error("expected string for " + string(name));
            field = v.asString();
        } else if constexpr (is_same_v<FieldType, double>) {
            if (!v.isNumber()) throw runtime_error("expected number for " + string(name));
            field = v.asNumber();
        } else if constexpr (is_same_v<FieldType, int>) {
            if (!v.isNumber()) throw runtime_error("expected number for " + string(name));
            field = static_cast<int>(v.asNumber());
        } else if constexpr (is_same_v<FieldType, vector<Value>>) {
            if (!v.isArray()) throw runtime_error("expected array for " + string(name));
            field = v.asArray();
        } else if constexpr (is_same_v<FieldType, Object>) {
            if (!v.isObject()) throw runtime_error("expected object for " + string(name));
            field = v.asObject();
        } else {
            field = fromValue<FieldType>(v);
        }
    });
    return obj;
}

template<typename T>
Value toValue(const T& obj) {
    Object o;
    obj.reflect([&](const char* name, const auto& field) {
        using FieldType = std::decay_t<decltype(field)>;
        if constexpr (is_same_v<FieldType, string>) {
            o[name] = field;
        } else if constexpr (is_same_v<FieldType, double>) {
            o[name] = field;
        } else if constexpr (is_same_v<FieldType, int>) {
            o[name] = static_cast<double>(field);
        } else if constexpr (is_same_v<FieldType, vector<Value>>) {
            o[name] = field;
        } else if constexpr (is_same_v<FieldType, Object>) {
            o[name] = field;
        } else {
            o[name] = toValue(field);
        }
    });
    return Value(o);
}

template<typename T>
Object toObject(const T& obj) {
    Value val = toValue(obj);
    if (!val.isObject()) throw std::runtime_error("toObject: expected object");
    return val.asObject();
}

static inline bool lngSaveToFile(const string& filename, const Value& val) {
    ofstream out(filename);
    if (!out) return false;
    out << prettySerialize(val);
    return true;
}

static inline bool lngLoadFromFile(const string& filename, Value& val) {
    ifstream in(filename);
    if (!in) return false;
    string content((std::istreambuf_iterator<char>(in)),
                         istreambuf_iterator<char>());
    try {
        Parser parser;
        val = parser.parse(content);
    } catch (...) {
        return false;
    }
    return true;
}

#define PlatformDependantBCFields(X) \
    X(string, compiler) \
    X(string, outputName) \
    X(Array, cxxFlags) \
    X(Array, ldFlags) \

ReflectStruct(PlatformDependantBC, PlatformDependantBCFields);

#define ModPlatformDependantBCFields(X) \
    X(string, compiler) \
    X(string, outputName) \
    X(Array, cxxFlags) \
    X(Array, ldFlags) \
    X(string, entrypoint) \

ReflectStruct(ModPlatformDependantBC, ModPlatformDependantBCFields);

#define BuildConfigProfilesFields(X) \
    X(Object, windows) \
    X(Object, mac) \
    X(Object, linux) \

ReflectStruct(BuildConfigProfiles, BuildConfigProfilesFields);

#define SelectProfile(X) \
    X.PLATFORM

#define BuildConfigFields(X) \
    X(string, srcDir) \
    X(string, targetDir) \
    X(string, objDir) \
    X(string, projType) \
    X(Array, watchDefExts) \
    X(string, entryPoint) \
    X(Object, profiles) \

#define BuildConfigDefaultFields(X) \
    X("./src") \
    X("./target") \
    X("./target/obj") \
    X("cpp") \
    X({".cpp", ".h", ".hpp"}) \
    X("./src/main.cpp") \

ReflectStruct(BuildConfig, BuildConfigFields);

constexpr const char* defaultSrcDir = "./src";
constexpr const char* defaultTargetDir = "./target";
constexpr const char* defaultObjDir = "./target/obj";
constexpr const char* defaultProjType = ".cpp";
const Array defaultWatchDefExts = {".cpp", ".h", ".hpp"};
constexpr const char* defaultEntryPoint = "./src/main.cpp";

constexpr const char* defaultWinCompiler = "cl.exe";
constexpr const char* defaultWinExeFileName = "holybine.exe";
const Array defaultWinCxxFlags = {"/std:c++17"};
const Array defaultWinLdFlags = {};

#define DefaultWinPlatformDepFields \
    toObject(PlatformDependantBC { \
            defaultWinCompiler, \
            defaultWinExeFileName, \
            defaultWinCxxFlags, \
            defaultWinLdFlags, \
    }) \

constexpr const char* defaultCompiler = "g++";
constexpr const char* defaultExeFileName = "holybine";
const Array defaultCxxFlags = {"-std=c++17"};
const Array defaultLdFlags = {};

#define DefaultPlatformDepFields \
    toObject(PlatformDependantBC { \
            defaultCompiler, \
            defaultExeFileName, \
            defaultCxxFlags, \
            defaultLdFlags, \
    }) \

inline bool getConfig(BuildConfig* config) {
    Value configVal;
    lngLoadFromFile(ConfigFile, configVal);

    if(!configVal.isObject()) {
        SwineLog(ERR, "expected object from " + string(ConfigFile) + " config file");
        return false;
    }

    *config = fromValue<BuildConfig>(configVal);

    return true;
}

template<typename T, typename B>
inline B getProfile(T config) {
    B profile = fromValue<B>(fromValue<BuildConfigProfiles>(config.profiles).PLATFORM);

    return profile;
}

inline optional<vector<string>> arrayToStringVector(const Array& arr) {
    vector<string> result;
    result.reserve(arr.size());

    for (const auto& val : arr) {
        if (!val.isString()) {
            return std::nullopt;
        }
        result.push_back(val.asString());
    }

    return result;
}

#define BuildExtensionOptionFields \
    X(build,          "build") \
    X(buildwindows,   "buildWindows") \
    X(outfolder,      "outfolder") \
    X(outname,        "outname")

#define ModuleOptionFields(X) \
    X(string, outfolder) \
    X(Array, addsources) \
    X(string, projtype) \
    X(Object, profiles) \

ReflectStruct(ModuleOptions, ModuleOptionFields);

#define ToString(thing) thing.string()

struct ModuleDiscovery {
    ModuleOptions opts;
    string entryPoint;

    ModuleDiscovery(ModuleOptions opt, string entry): opts(opt), entryPoint(entry) {}
};

struct FindDiscoveryRes {
    vector<string> sources;
    vector<ModuleDiscovery> modules;
};

static inline FindDiscoveryRes SwineGetFiles() {
    BuildConfig config;

    if (!getConfig(&config)) {
        SwineLog(ERR, "failed to get the config");
        exit(1);
    }

    vector<ModuleDiscovery> modules;
    vector<string> sources;
    vector<string> sourceAddOns;

    for (const auto& entry : fs::recursive_directory_iterator(config.srcDir)) {
        fs::path ep = entry.path();

        if (fs::is_directory(ep)) {
            for (const auto& module : fs::recursive_directory_iterator(ep)) {
                fs::path mp = module.path();

                if (mp.stem() == ".build") {
                    Value moduleConfig;

                    if (!lngLoadFromFile(mp, moduleConfig)) {
                        SwineLog(ERR, "malformed module file");
                        break;
                    }

                    ModuleOptions opts = fromValue<ModuleOptions>(moduleConfig);

                    ModPlatformDependantBC moduleProfile = getProfile<ModuleOptions, ModPlatformDependantBC>(opts);

                    fs::path moduleParentPath = mp.parent_path();

                    string moduleEntry = ToString(moduleParentPath)+ "/" + moduleProfile.entrypoint;

                    if (!fs::exists(moduleEntry)) {
                        SwineLog(ERR, "cannot build module '" + ToString(moduleParentPath) + "' your .build module must contain a valid entry point");
                        break;
                    } 
                    if (moduleProfile.outputName.empty()) {
                        SwineLog(ERR, "cannot build module '" + ToString(moduleParentPath) + "' you must provide an outname for at least your platform in the .build module");
                        break;
                    }

                    modules.emplace_back(opts, moduleEntry);
                }
            }
        } else {
            fs::path parentStem = entry.path().parent_path().stem();
            string parentStemStr = ToString(parentStem);

            bool skip = any_of(modules.begin(), modules.end(), [&](const ModuleDiscovery& mod) {
                return fs::path(mod.entryPoint).parent_path().stem() == parentStemStr;
            });

            auto ext = entry.path().extension();

            if (ext == config.projType && !startsWith(ext.string(), "._") && !skip) {
                sources.push_back(entry.path().string());
            }
        }
    }

    return { sources, modules };
}

static inline void BuildCompileCommands() {
    SwineLog(INFO, "generating compile_commands.json");

    Task ninjaCompileCmds = {{"ninja", "-t", "compdb", "cxx", "cc"}};
    ninjaCompileCmds.run(true);

    ofstream coc("compile_commands.json");

    coc << ninjaCompileCmds.output;
    coc.close();

    SwineLog(INFO, "compile_commands.json generated");
}

#define SwineWatchTemplate(name, callback) \
    void name(int argc, char** argv) { \
        BuildConfig config; \
        \
        if (!getConfig(&config)) { \
            SwineLog(ERR, "failed to get the config"); \
            exit(1); \
        } \
        \
        callback; \
        \
        vector<string> validExt = {}; \
        if (auto opt = arrayToStringVector(config.watchDefExts)) { \
            validExt = std::move(*opt); \
        } else { \
            SwineLog(ERR, "CxxFlags in config are malformed"); \
            exit(1); \
        } \
        \
        map<string, fs::file_time_type> fileMod; \
        map<string, int> missingCount; \
        const int removalThreshold = 3; \
        \
        auto populateFiles = [&]() -> vector<string> { \
            vector<string> files; \
            for (const auto& entry : fs::recursive_directory_iterator(config.srcDir)) { \
                for (const auto& allowed : validExt) { \
                    auto ext = entry.path().extension(); \
                    if (ext == allowed && !startsWith(ext, "._")) { \
                        string filePath = entry.path().string(); \
                        files.push_back(filePath); \
                        if (fileMod.find(filePath) == fileMod.end()) { \
                            try { \
                                fileMod[filePath] = fs::last_write_time(entry.path()); \
                            } catch (const fs::filesystem_error& e) { \
                                SwineLog(WARN, "error retrieving last write time for " + filePath + ": " + e.what()); \
                            } \
                        } \
                        break; \
                    } \
                } \
            } \
            return files; \
        }; \
        \
        vector<string> files = populateFiles(); \
        SwineLog(INFO, "starting to watch over '" + string(config.srcDir) + "'"); \
        \
        for (;;) { \
            vector<string> newFiles = populateFiles(); \
            \
            for (const auto& newFile : newFiles) { \
                if (fileMod.find(newFile) == fileMod.end()) { \
                    SwineLog(INFO, "new file added: '" + newFile + "'"); \
                    try { \
                        fileMod[newFile] = fs::last_write_time(newFile); \
                    } catch (const fs::filesystem_error& e) { \
                        SwineLog(WARN, "error retrieving last write time for new file " + newFile + ": " + e.what()); \
                    } \
                    missingCount[newFile] = 0; \
                    callback; \
                } \
            } \
            \
            for (auto it = fileMod.begin(); it != fileMod.end(); ) { \
                if (find(newFiles.begin(), newFiles.end(), it->first) == newFiles.end()) { \
                    missingCount[it->first]++; \
                    if (missingCount[it->first] >= removalThreshold) { \
                        SwineLog(INFO, "file removed: '" + it->first + "'"); \
                        missingCount.erase(it->first); \
                        it = fileMod.erase(it); \
                        callback; \
                    } else { \
                        ++it; \
                    } \
                } else { \
                    missingCount[it->first] = 0; \
                    ++it; \
                } \
            } \
            \
            for (const auto& entry : fileMod) { \
                try { \
                    auto currentModTime = fs::last_write_time(entry.first); \
                    if (currentModTime != entry.second) { \
                        SwineLog(INFO, "file modified: '" + entry.first + "' regenerating ninja script"); \
                        fileMod[entry.first] = currentModTime; \
                        callback; \
                    } \
                } catch (const fs::filesystem_error& e) { \
                    SwineLog(WARN, "failed to get last_write_time for " + entry.first + ": " + e.what()); \
                } \
            } \
            \
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); \
        } \
    } \


inline void ConfigSetup() {
    if (!fs::exists(ConfigFile)) {
        BuildConfig config = {
            defaultSrcDir,
            defaultTargetDir,
            defaultObjDir,
            defaultProjType,
            defaultWatchDefExts,
            defaultEntryPoint,
            toObject(BuildConfigProfiles{
                    DefaultWinPlatformDepFields,
                    DefaultPlatformDepFields,
                    DefaultPlatformDepFields,
                    })
        };

        lngSaveToFile(ConfigFile, toValue(config));
    }

    BuildConfig config;

    if (!getConfig(&config)) {
        SwineLog(ERR, "failed to get the config");
        exit(1);
    }

    if (!fs::exists(config.srcDir) || !fs::is_directory(config.srcDir)) {
        SwineLog(ERR, "source directory: '" + string(config.srcDir) + "' doesnt exist") ;
        exit(69);
    }
    
    CheckDirCreateIfNot(config.targetDir)
    CheckDirCreateIfNot(config.objDir)
}

#define SwineBuildMain(callback) \
    int main(int argc, char** argv) { \
        GoRebuildYourself(argc, argv); \
        ConfigSetup(); \
        SwineCli app; \
        callback; \
        app.go(argc, argv); \
        return 0; \
    }
 
#endif
