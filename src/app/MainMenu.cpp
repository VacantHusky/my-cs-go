#include "app/MainMenu.h"

namespace mycsg::app {

MainMenuModel::MainMenuModel()
    : title_("全民竞技实验场"),
      subtitle_("Vulkan 战术射击原型：支持单机、联机、地图编辑与后续模式扩展。"),
      sections_({
          MenuSection{
              .title = "开始游戏",
              .items = {
                  MenuItem{"singleplayer", "单机模式", "本地训练、机器人对战、离线调试。", AppFlow::SinglePlayerLobby},
                  MenuItem{"multiplayer", "联机模式", "局域网或专用服务器房间。", AppFlow::MultiPlayerLobby},
                  MenuItem{"editor", "地图编辑器", "可视化摆放方块、出生点、掩体与道具。", AppFlow::MapEditor},
              },
          },
          MenuSection{
              .title = "系统",
              .items = {
                  MenuItem{"settings", "设置", "图像、音频、操作与网络参数。", AppFlow::Settings},
                  MenuItem{"exit", "退出游戏", "关闭应用程序。", AppFlow::Exit},
              },
          },
      }) {
    for (const auto& section : sections_) {
        for (const auto& item : section.items) {
            items_.push_back(&item);
        }
    }
}

const MenuItem* MainMenuModel::itemAt(const std::size_t index) const {
    if (index >= items_.size()) {
        return nullptr;
    }
    return items_[index];
}

}  // namespace mycsg::app
