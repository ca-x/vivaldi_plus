#include <map>
#include <string>

std::map<std::string, std::string> parseYaml(std::string yamlFilePath) {
    std::map<std::string, std::string> parameters;
    // Implement YAML parsing using the standard library
    // Add error handling
    return parameters;
}

int main(int argc, char* argv[]) {
    std::map<std::string, std::string> parameters = parseYaml("parameters.yaml");
    std::string startupCommand = "./vivaldi";
    for (auto const& param : parameters) {
        startupCommand += " --" + param.first + "=" + param.second;
    }
    ShellExecute(NULL, "open", startupCommand.c_str(), NULL, NULL, SW_SHOWDEFAULT);
}