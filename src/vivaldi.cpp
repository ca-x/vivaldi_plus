// The yaml-cpp library is a required dependency for this project.
// To install the library, you can use the following command: 
// 'sudo apt-get install libyaml-cpp-dev'
// Make sure to configure your project settings to include the library's header files.
#include <yaml-cpp/yaml.h>
#include <map>
#include <string>

std::map<std::string, std::string> getParametersFromYaml(std::string yamlFilePath) {
    std::map<std::string, std::string> parameters;
    try {
        YAML::Node config = YAML::LoadFile(yamlFilePath);
        for(YAML::const_iterator it=config.begin();it!=config.end();++it) {
            parameters[it->first.as<std::string>()] = it->second.as<std::string>();
        }
    } catch (YAML::BadFile e) {
        DebugLog(L"Error: Unable to open the YAML file. Please ensure that the yaml-cpp library is installed and the YAML file exists.");
    } catch (YAML::ParserException e) {
        DebugLog(L"Error: Unable to parse the YAML file. Please ensure that the yaml-cpp library is correctly configured and the YAML file is correctly formatted.");
    }
    return parameters;
}

int main(int argc, char* argv[]) {
    std::map<std::string, std::string> parameters = getParametersFromYaml("parameters.yaml");
    std::string startupCommand = "./vivaldi";
    for (auto const& param : parameters) {
        startupCommand += " --" + param.first + "=" + param.second;
    }
    ShellExecute(NULL, "open", startupCommand.c_str(), NULL, NULL, SW_SHOWDEFAULT);
}