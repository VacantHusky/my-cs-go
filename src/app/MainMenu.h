#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace mycsg::app {

enum class AppFlow {
    MainMenu,
    MapBrowser,
    SinglePlayerLobby,
    MultiPlayerLobby,
    MapEditor,
    Settings,
    Exit
};

struct MenuItem {
    std::string id;
    std::string label;
    std::string description;
    AppFlow target = AppFlow::MainMenu;
};

struct MenuSection {
    std::string title;
    std::vector<MenuItem> items;
};

class MainMenuModel {
public:
    MainMenuModel();

    const std::string& title() const { return title_; }
    const std::string& subtitle() const { return subtitle_; }
    const std::vector<MenuSection>& sections() const { return sections_; }
    const std::vector<const MenuItem*>& items() const { return items_; }
    const MenuItem* itemAt(const std::size_t index) const;

private:
    std::string title_;
    std::string subtitle_;
    std::vector<MenuSection> sections_;
    std::vector<const MenuItem*> items_;
};

}  // namespace mycsg::app
