#include "app/application.h"

#include <iostream>
#include <string>

int main(int argc, char ** argv) {
    try {
        RuntimeConfig config;

        if (argc >= 2) {
            config.model_path = argv[1];
        }

        if (argc >= 3) {
            config.shader_dir = argv[2];
        }

        Application application(config);
        return application.run();
    } catch (const std::exception & e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
