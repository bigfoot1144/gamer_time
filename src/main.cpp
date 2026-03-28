#include "app.h"

#include <iostream>
#include <string>

int main(int argc, char ** argv) {
    try {
        std::string model_path;
        std::string shader_dir = "shaders";

        if (argc >= 2) {
            model_path = argv[1];
        }

        if (argc >= 3) {
            shader_dir = argv[2];
        }

        App app(model_path, shader_dir);
        return app.run();
    } catch (const std::exception & e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
