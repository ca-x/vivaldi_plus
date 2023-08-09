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
        // Handle exception
    } catch (YAML::ParserException e) {
        // Handle exception
    }
    return parameters;
}

int main(int argc, char* argv[]) {
    std::map<std::string, std::string> parameters = getParametersFromYaml("parameters.yaml");
    std::string startupCommand = "./vivaldi";
    for (auto const& param : parameters) {
        startupCommand += " --" + param.first + "=" + param.second;
    }
    system(startupCommand.c_str());
}