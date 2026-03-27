    void syncMultiplayerDraft(const RenderFrame& renderFrame) {
        if (multiplayerDraftInitialized_) {
            return;
        }
        multiplayerDraftInitialized_ = true;
        multiplayerSessionTypeDraft_ = renderFrame.multiplayerSessionTypeIndex;
        multiplayerPortDraft_ = renderFrame.multiplayerPort;
        multiplayerMaxPlayersDraft_ = renderFrame.multiplayerMaxPlayers;
        copyStringToBuffer(multiplayerHostDraft_, renderFrame.multiplayerHost);
    }

    void emitMultiplayerDraftChanges(const RenderFrame& renderFrame) {
        if (multiplayerSessionTypeDraft_ != renderFrame.multiplayerSessionTypeIndex) {
            queueUiAction(UiActionType::SetMultiplayerSessionType, multiplayerSessionTypeDraft_);
        }

        const std::string draftHost(multiplayerHostDraft_.data());
        if (draftHost != renderFrame.multiplayerHost) {
            queueUiTextAction(UiActionType::SetMultiplayerHost, draftHost);
        }

        if (multiplayerPortDraft_ != renderFrame.multiplayerPort) {
            queueUiAction(UiActionType::SetMultiplayerPort, multiplayerPortDraft_);
        }

        if (multiplayerMaxPlayersDraft_ != renderFrame.multiplayerMaxPlayers) {
            queueUiAction(UiActionType::SetMultiplayerMaxPlayers, multiplayerMaxPlayersDraft_);
        }
    }

    bool beginTwoColumnFormTable(const char* tableId, const float labelWidth) {
        constexpr ImGuiTableFlags kFormTableFlags =
            ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_BordersInnerV;
        if (!ImGui::BeginTable(tableId, 2, kFormTableFlags)) {
            return false;
        }
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    void nextTwoColumnFormRow(const char* label) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
    }

    void drawMultiplayerModeRow() {
        nextTwoColumnFormRow("模式");
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(18.0f, 10.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.24f, 0.31f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.38f, 0.50f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.28f, 0.48f, 0.64f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.98f, 0.86f, 0.48f, 1.00f));
        if (ImGui::RadioButton("主机##mp_mode_host", multiplayerSessionTypeDraft_ == 0)) {
            multiplayerSessionTypeDraft_ = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("客户端##mp_mode_client", multiplayerSessionTypeDraft_ == 1)) {
            multiplayerSessionTypeDraft_ = 1;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    void drawMultiplayerMapRow(const RenderFrame& renderFrame) {
        nextTwoColumnFormRow("地图");
        const char* currentMapPreview = renderFrame.mapBrowserItems.empty()
            ? "没有可用地图"
            : renderFrame.mapBrowserItems[std::min(renderFrame.mapBrowserSelectedIndex, renderFrame.mapBrowserItems.size() - 1)].c_str();
        const bool hasAnyMap = !renderFrame.mapBrowserItems.empty();
        if (!hasAnyMap) {
            ImGui::BeginDisabled();
        }
        if (ImGui::BeginCombo("##mp_map", currentMapPreview)) {
            for (std::size_t index = 0; index < renderFrame.mapBrowserItems.size(); ++index) {
                const bool selected = index == renderFrame.mapBrowserSelectedIndex;
                if (ImGui::Selectable(renderFrame.mapBrowserItems[index].c_str(), selected)) {
                    queueUiAction(UiActionType::SelectMapBrowserItem, static_cast<std::int32_t>(index));
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (!hasAnyMap) {
            ImGui::EndDisabled();
        }
    }

    void drawMultiplayerForm(const RenderFrame& renderFrame) {
        if (!beginTwoColumnFormTable("multiplayer_form", 108.0f)) {
            return;
        }

        drawMultiplayerModeRow();
        drawMultiplayerMapRow(renderFrame);

        nextTwoColumnFormRow("服务器");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##mp_host", "127.0.0.1", multiplayerHostDraft_.data(), multiplayerHostDraft_.size());

        nextTwoColumnFormRow("端口");
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputInt("##mp_port", &multiplayerPortDraft_, 1, 100)) {
            multiplayerPortDraft_ = std::clamp(multiplayerPortDraft_, 1, 65535);
        }

        nextTwoColumnFormRow("房间人数");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##mp_max_players", &multiplayerMaxPlayersDraft_, 2, 32, "%d 人");

        nextTwoColumnFormRow("当前地址");
        ImGui::TextUnformatted(renderFrame.multiplayerEndpointLabel.c_str());

        nextTwoColumnFormRow("状态");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(renderFrame.multiplayerStatusLabel.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndTable();
    }

    void drawMultiplayerActions(const RenderFrame& renderFrame) {
        ImGui::SeparatorText("操作");
        const float buttonWidth = (ImGui::GetContentRegionAvail().x - 12.0f) / 3.0f;
        if (ImGui::Button("应用网络参数", ImVec2(buttonWidth, 0.0f))) {
            emitMultiplayerDraftChanges(renderFrame);
        }
        ImGui::SameLine();
        if (ImGui::Button(renderFrame.multiplayerSessionActive ? "重新启动会话" : "启动会话", ImVec2(buttonWidth, 0.0f))) {
            emitMultiplayerDraftChanges(renderFrame);
            queueUiAction(UiActionType::ActivateMultiplayerSetting, 3);
        }
        ImGui::SameLine();
        if (ImGui::Button("返回主菜单", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::ReturnToMainMenu);
        }
    }

    void buildMultiplayerWindow(const RenderFrame& renderFrame) {
        syncMultiplayerDraft(renderFrame);
        ImGui::SetNextWindowPos(ImVec2(42.0f, 38.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(580.0f, 420.0f), ImGuiCond_Always);
        constexpr ImGuiWindowFlags kMultiplayerWindowFlags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("联机房间工具", nullptr, kMultiplayerWindowFlags)) {
            ImGui::Text("地图  %s", renderFrame.multiplayerMapLabel.c_str());
            ImGui::SeparatorText("房间参数");
            drawMultiplayerForm(renderFrame);
            drawMultiplayerActions(renderFrame);
        }
        ImGui::End();
    }
