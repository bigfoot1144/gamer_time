#pragma once

#include <string>

class TextOverlayRenderer {
public:
    void set_text(std::string text);

    const std::string & text() const {
        return text_;
    }

    bool take_dirty();

private:
    std::string text_;
    bool dirty_ = true;
};
