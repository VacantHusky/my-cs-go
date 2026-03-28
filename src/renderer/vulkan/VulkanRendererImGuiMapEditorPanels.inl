    enum class MapEditorTopTab {
        Scene = 0,
        MapInfo = 1,
        Assets = 2,
    };

    static constexpr const char* kIconCrosshairs = "\xEF\x81\x9B";
    static constexpr const char* kIconHand = "\xEF\x89\x95";
    static constexpr const char* kIconPlus = "\x2B";
    static constexpr const char* kIconTrash = "\xEF\x87\xB8";
    static constexpr const char* kIconCamera = "\xEF\x80\xB0";
    static constexpr const char* kIconMesh = "\xEF\x97\xAE";
    static constexpr const char* kIconCube = "\xEF\x86\xB2";
    static constexpr const char* kIconExpand = "\xEF\x81\xA5";
    static constexpr std::array<const char*, 6> kManagedObjectCategories{
        "大型建筑",
        "小型建筑",
        "建筑部件",
        "人造物体",
        "自然物体",
        "标记与玩法",
    };

    std::string buildMapEditorToolHint(const RenderFrame& renderFrame) const {
        if (renderFrame.editorToolLabel == "抓手") {
            return renderFrame.editorIsOrthoView
                ? "抓手工具: 浏览场景，WASD 平移，滚轮缩放"
                : "抓手工具: 浏览场景，右键环视，WASD 飞行";
        }
        if (renderFrame.editorToolLabel == "放置") {
            return "左键/Enter 放置，R 旋转，Tab 切换缩放";
        }
        if (renderFrame.editorToolLabel == "擦除") {
            return "点击删除红色高亮对象";
        }
        if (renderFrame.editorIsOrthoView) {
            return "选择工具: 点击对象选中，WASD 平移，滚轮缩放";
        }
        return "选择工具: 镜头正前方自动选中，右键环视，WASD 飞行";
    }

    bool drawFloatingToolButton(const char* id,
                                const char* glyph,
                                const char* tooltip,
                                const bool active,
                                const float diameter = 34.0f) {
        ImGui::PushID(id);
        ImGui::InvisibleButton("##floating-icon", ImVec2(diameter, diameter));
        const bool pressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImVec2 center{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
        const float radius = diameter * 0.5f;
        const ImU32 fillColor = ImGui::GetColorU32(
            active
                ? (held ? ImVec4(0.24f, 0.42f, 0.56f, 1.0f) : ImVec4(0.26f, 0.48f, 0.62f, 0.95f))
                : (hovered ? ImVec4(0.15f, 0.18f, 0.24f, 0.96f) : ImVec4(0.08f, 0.10f, 0.14f, 0.90f)));
        const ImU32 borderColor = ImGui::GetColorU32(
            active ? ImVec4(0.70f, 0.87f, 0.98f, 0.95f) : ImVec4(1.0f, 1.0f, 1.0f, hovered ? 0.28f : 0.16f));
        const ImU32 iconColor = ImGui::GetColorU32(
            active ? ImVec4(0.96f, 0.99f, 1.0f, 1.0f) : ImVec4(0.92f, 0.95f, 0.98f, hovered ? 0.98f : 0.88f));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddCircleFilled(center, radius, fillColor, 24);
        drawList->AddCircle(center, radius - 0.5f, borderColor, 24, active ? 1.8f : 1.2f);
        const ImVec2 textSize = ImGui::CalcTextSize(glyph);
        drawList->AddText(
            ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f - 1.0f),
            iconColor,
            glyph);
        if (ImGui::IsItemHovered() && tooltip != nullptr && tooltip[0] != '\0') {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::PopID();
        return pressed;
    }

    void drawMapEditorSceneCollectionSection(const RenderFrame& renderFrame) {
        ImGui::SeparatorText("场景集合");
        if (renderFrame.editingMap == nullptr) {
            ImGui::TextDisabled("当前没有可编辑场景。");
            return;
        }

        const auto& props = renderFrame.editingMap->props;
        ImGui::InputTextWithHint(
            "##editor-scene-filter", "筛选名称、id、分类...", editorSceneFilter_.data(), editorSceneFilter_.size());
        ImGui::TextDisabled("对象 %zu", props.size());
        ImGui::BeginChild("editor-scene-object-list", ImVec2(0.0f, 228.0f), true);
        if (props.empty()) {
            ImGui::TextDisabled("当前场景没有对象。");
            ImGui::EndChild();
            return;
        }

        const std::string filter = lowerAsciiCopy(editorSceneFilter_.data());
        for (std::size_t index = 0; index < props.size(); ++index) {
            const auto& prop = props[index];
            const std::string haystack = lowerAsciiCopy(prop.label + " " + prop.id + " " + prop.category);
            if (!filter.empty() && haystack.find(filter) == std::string::npos) {
                continue;
            }
            std::string label = prop.label.empty() ? prop.id : prop.label;
            if (!prop.id.empty() && prop.id != label) {
                label += " | ";
                label += prop.id;
            }
            if (!prop.category.empty()) {
                label += " | ";
                label += prop.category;
            }
            ImGui::PushID(static_cast<int>(index));
            const bool selected = renderFrame.selectedEditorPropIndex == static_cast<int>(index);
            if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(0.0f, 22.0f))) {
                queueUiAction(UiActionType::SelectSceneEditorProp, static_cast<std::int32_t>(index));
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("位置 %.2f, %.2f, %.2f",
                    prop.position.x,
                    prop.position.y,
                    prop.position.z);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    void drawMapEditorObjectEditingSection(const RenderFrame& renderFrame, const EditorAssetSelectionState& assetState) {
        if (renderFrame.hasSelectedEditorProp) {
            drawMapEditorInspectorSection(renderFrame);
            return;
        }

        if (renderFrame.editorToolLabel == "放置") {
            ImGui::SeparatorText("放置设置");
            drawMapEditorPlacementSection(renderFrame, assetState);
            return;
        }

        ImGui::SeparatorText("对象参数");
        ImGui::TextDisabled("%s",
            renderFrame.editorIsOrthoView
                ? "选择工具下点击场景中的对象，或从上方列表选择对象。"
                : "选择工具下会自动锁定镜头正前方对象，也可以从上方列表定位。");
    }

    void drawFloatingMapEditorToolWindow(const RenderFrame& renderFrame,
                                         const ImVec2 position) {
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        constexpr ImGuiWindowFlags kFloatingFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 4.0f));
        if (ImGui::Begin("地图编辑器左侧浮动工具", nullptr, kFloatingFlags)) {
            static constexpr std::array<const char*, 4> kToolGlyphs{
                kIconCrosshairs,
                kIconHand,
                kIconPlus,
                kIconTrash,
            };
            static constexpr std::array<const char*, 4> kToolNames{
                "选择",
                "抓手",
                "放置",
                "擦除",
            };
            for (std::size_t index = 0; index < kToolGlyphs.size(); ++index) {
                const bool selected = renderFrame.editorToolLabel == kToolNames[index];
                if (drawFloatingToolButton(
                        ("map-editor-tool-" + std::to_string(index)).c_str(),
                        kToolGlyphs[index],
                        kToolNames[index],
                        selected,
                        32.0f)) {
                    const std::int32_t toolIndex = index == 3
                        ? 3
                        : static_cast<std::int32_t>(index);
                    queueUiAction(UiActionType::SelectMapEditorTool, toolIndex);
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void drawFloatingMapEditorViewWindow(const RenderFrame& renderFrame,
                                         const ImVec2 position) {
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        constexpr ImGuiWindowFlags kFloatingFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));
        if (ImGui::Begin("地图编辑器右上浮动工具", nullptr, kFloatingFlags)) {
            if (drawFloatingToolButton(
                    "map-editor-view-projection",
                    kIconCamera,
                    renderFrame.editorIsOrthoView ? "切到自由镜头" : "切到 2.5D 正交",
                    false,
                    32.0f)) {
                queueUiAction(UiActionType::ToggleMapEditorProjection);
            }
            ImGui::SameLine();
            if (drawFloatingToolButton(
                    "map-editor-view-mesh",
                    kIconMesh,
                    renderFrame.editorShowMeshOutline ? "隐藏真实轮廓" : "显示真实轮廓",
                    renderFrame.editorShowMeshOutline,
                    32.0f)) {
                queueUiAction(UiActionType::ToggleMapEditorMeshOutline, renderFrame.editorShowMeshOutline ? 0 : 1);
            }
            ImGui::SameLine();
            if (drawFloatingToolButton(
                    "map-editor-view-collision",
                    kIconCube,
                    renderFrame.editorShowCollisionOutline ? "隐藏碰撞箱轮廓" : "显示碰撞箱轮廓",
                    renderFrame.editorShowCollisionOutline,
                    32.0f)) {
                queueUiAction(UiActionType::ToggleMapEditorCollisionOutline, renderFrame.editorShowCollisionOutline ? 0 : 1);
            }
            ImGui::SameLine();
            if (drawFloatingToolButton(
                    "map-editor-view-bounds",
                    kIconExpand,
                    renderFrame.editorShowBoundingBox ? "隐藏最小包裹箱" : "显示最小包裹箱",
                    renderFrame.editorShowBoundingBox,
                    32.0f)) {
                queueUiAction(UiActionType::ToggleMapEditorBoundingBox, renderFrame.editorShowBoundingBox ? 0 : 1);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void drawMapEditorInspectorSection(const RenderFrame& renderFrame) {
        ImGui::SeparatorText("对象参数");
        if (renderFrame.hasSelectedEditorProp) {
            ImGui::Text("名称: %s", renderFrame.selectedEditorPropLabel.c_str());
            ImGui::TextDisabled("ID: %s", renderFrame.selectedEditorPropAssetId.c_str());
            if (ImGui::BeginTable("editor-prop-summary", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("分类: %s", renderFrame.selectedEditorPropCategoryLabel.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("模型: %s", renderFrame.selectedEditorPropModelLabel.c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("材质: %s", renderFrame.selectedEditorPropMaterialLabel.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(" ");
                ImGui::EndTable();
            }
            if (ImGui::BeginTable("editor-prop-inspector", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                auto drawVec3Editor = [&](const char* label,
                                          const char* widgetId,
                                          const util::Vec3& value,
                                          const float speed,
                                          const float minValue,
                                          const float maxValue,
                                          const UiActionType actionType,
                                          const char* format) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::TableSetColumnIndex(1);
                    float components[3]{value.x, value.y, value.z};
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::DragFloat3(widgetId, components, speed, minValue, maxValue, format)) {
                        queueUiVec3Action(actionType, util::Vec3{components[0], components[1], components[2]});
                    }
                };

                drawVec3Editor("位置", "##selected-prop-position", renderFrame.selectedEditorPropPosition,
                    0.05f, -64.0f, 128.0f, UiActionType::SetSelectedEditorPropPosition, "%.2f");
                drawVec3Editor("旋转", "##selected-prop-rotation", renderFrame.selectedEditorPropRotationDegrees,
                    1.0f, -360.0f, 360.0f, UiActionType::SetSelectedEditorPropRotation, "%.0f deg");
                drawVec3Editor("缩放", "##selected-prop-scale", renderFrame.selectedEditorPropScale,
                    0.05f, 0.05f, 8.0f, UiActionType::SetSelectedEditorPropScale, "%.2f");
                ImGui::EndTable();
            }
        } else {
            ImGui::TextWrapped("当前没有选中的可编辑对象。切到“选择”工具并把鼠标指向场景道具后，这里会显示参数。");
        }
    }

    void drawMapEditorTopBarContent(const RenderFrame& renderFrame) {
        if (!ImGui::BeginMenuBar()) {
            return;
        }

        if (ImGui::BeginMenu("文件")) {
            if (ImGui::MenuItem("新建")) {
                queueUiAction(UiActionType::CreateEditorMap);
            }
            if (ImGui::BeginMenu("打开...")) {
                for (std::size_t index = 0; index < renderFrame.mapBrowserItems.size(); ++index) {
                    const bool selected = index == renderFrame.mapBrowserSelectedIndex;
                    if (ImGui::MenuItem(renderFrame.mapBrowserItems[index].c_str(), nullptr, selected)) {
                        queueUiAction(UiActionType::SelectMapBrowserItem, static_cast<std::int32_t>(index));
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("保存")) {
                queueUiAction(UiActionType::SaveEditorMap);
            }
            if (ImGui::MenuItem("返回主菜单")) {
                queueUiAction(UiActionType::ReturnToMainMenu);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("编辑")) {
            const bool canUndo = renderFrame.editorUndoAvailable;
            if (!canUndo) {
                ImGui::BeginDisabled();
            }
            if (ImGui::MenuItem("撤销")) {
                queueUiAction(UiActionType::UndoMapEditorChange);
            }
            if (!canUndo) {
                ImGui::EndDisabled();
            }
            ImGui::EndMenu();
        }

        static constexpr std::array<const char*, 3> kTopTabLabels{
            "场景编辑",
            "地图信息",
            "资产管理",
        };
        ImGui::Separator();
        for (std::size_t index = 0; index < kTopTabLabels.size(); ++index) {
            if (index > 0) {
                ImGui::SameLine();
            }
            const bool selected = static_cast<int>(selectedMapEditorTopTab_) == static_cast<int>(index);
            if (ImGui::Selectable(kTopTabLabels[index], selected, 0, ImVec2(74.0f, 0.0f))) {
                selectedMapEditorTopTab_ = static_cast<MapEditorTopTab>(index);
            }
        }

        const float statusWidth = ImGui::CalcTextSize(renderFrame.editorMapFileLabel.c_str()).x +
            ImGui::CalcTextSize(renderFrame.editorToolLabel.c_str()).x +
            ImGui::CalcTextSize(renderFrame.editorViewModeLabel.c_str()).x + 96.0f;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth > statusWidth + 16.0f) {
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - statusWidth);
            ImGui::TextDisabled("%s | %s / %s",
                renderFrame.editorMapFileLabel.c_str(),
                renderFrame.editorToolLabel.c_str(),
                renderFrame.editorViewModeLabel.c_str());
        }
        ImGui::EndMenuBar();
    }

    void drawMapEditorBottomStatusContent(const RenderFrame& renderFrame) {
        std::array<char, 128> cameraBuffer{};
        std::snprintf(cameraBuffer.data(), cameraBuffer.size(), "相机 %.1f, %.1f, %.1f",
            renderFrame.cameraPosition.x,
            renderFrame.cameraPosition.y,
            renderFrame.cameraPosition.z);
        std::array<char, 128> targetBuffer{};
        std::snprintf(targetBuffer.data(), targetBuffer.size(), "目标 %.2f, %.2f, %.2f",
            renderFrame.editorTargetPosition.x,
            renderFrame.editorTargetPosition.y,
            renderFrame.editorTargetPosition.z);
        const std::string selectedObjectId =
            renderFrame.hasSelectedEditorProp && !renderFrame.selectedEditorPropAssetId.empty()
                ? renderFrame.selectedEditorPropAssetId
                : "无";
        const std::string toolHint = buildMapEditorToolHint(renderFrame);
        const char* undoLabel = renderFrame.editorUndoAvailable ? "可撤销" : "无撤销";

        const float textLineHeight = ImGui::GetTextLineHeight();
        const float centeredY = std::max(
            0.0f,
            (ImGui::GetWindowHeight() - textLineHeight) * 0.5f - ImGui::GetStyle().WindowPadding.y);
        if (centeredY > ImGui::GetCursorPosY()) {
            ImGui::SetCursorPosY(centeredY);
        }
        struct StatusSegment {
            std::string text;
            bool dim = true;
            bool required = false;
        };

        const std::vector<StatusSegment> segments{
            {std::string("状态: ") + renderFrame.editorStatusLabel, false, true},
            {std::string("选中 ") + selectedObjectId, true, true},
            {std::string(undoLabel), true, true},
            {std::string("提示 ") + toolHint, true, false},
            {cameraBuffer.data(), true, false},
            {targetBuffer.data(), true, false},
            {std::string("对象 ") + std::to_string(renderFrame.editorPropCount), true, false},
            {std::string("出生点 ") + std::to_string(renderFrame.editorSpawnCount), true, false},
        };

        float usedWidth = 0.0f;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float separatorWidth = ImGui::CalcTextSize("|").x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        bool first = true;
        for (const auto& segment : segments) {
            const float textWidth = ImGui::CalcTextSize(segment.text.c_str()).x;
            const float requiredWidth = textWidth + (first ? 0.0f : separatorWidth);
            const bool fits = usedWidth + requiredWidth <= availableWidth;
            if (!fits && !segment.required) {
                continue;
            }
            if (!first) {
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
            }
            if (segment.dim) {
                ImGui::TextDisabled("%s", segment.text.c_str());
            } else {
                ImGui::Text("%s", segment.text.c_str());
            }
            usedWidth += requiredWidth;
            first = false;
        }
    }

    void drawMapEditorMapInfoSection(const RenderFrame& renderFrame) {
        const auto drawSummaryCard = [](const char* title,
                                        const std::initializer_list<std::pair<const char*, std::string>>& items) {
            ImGui::BeginChild(title, ImVec2(0.0f, 0.0f), true);
            ImGui::TextUnformatted(title);
            ImGui::Separator();
            for (const auto& [label, value] : items) {
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine();
                ImGui::TextWrapped("%s", value.c_str());
            }
            ImGui::EndChild();
        };

        std::array<char, 128> mapIndexBuffer{};
        std::snprintf(mapIndexBuffer.data(), mapIndexBuffer.size(), "%zu / %zu",
            renderFrame.editorMapIndex + 1,
            std::max<std::size_t>(renderFrame.editorMapCount, 1));
        std::array<char, 128> cameraBuffer{};
        std::snprintf(cameraBuffer.data(), cameraBuffer.size(), "%.1f, %.1f, %.1f",
            renderFrame.cameraPosition.x,
            renderFrame.cameraPosition.y,
            renderFrame.cameraPosition.z);
        std::array<char, 64> objectAssetBuffer{};
        std::snprintf(objectAssetBuffer.data(), objectAssetBuffer.size(), "%d", renderFrame.editorObjectAssetCount);
        std::array<char, 64> categoryBuffer{};
        std::snprintf(categoryBuffer.data(), categoryBuffer.size(), "%d", renderFrame.editorObjectCategoryCount);
        std::array<char, 64> propBuffer{};
        std::snprintf(propBuffer.data(), propBuffer.size(), "%d", renderFrame.editorPropCount);
        std::array<char, 64> spawnBuffer{};
        std::snprintf(spawnBuffer.data(), spawnBuffer.size(), "%d", renderFrame.editorSpawnCount);

        if (ImGui::BeginTable("editor-map-summary-layout", 2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("meta", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("stats", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            drawSummaryCard("地图摘要", {
                {"地图文件", renderFrame.editorMapFileLabel},
                {"对象清单", renderFrame.editorAssetManifestLabel},
                {"当前工具", renderFrame.editorToolLabel},
                {"当前视图", renderFrame.editorViewModeLabel},
                {"编辑状态", renderFrame.editorStatusLabel},
            });

            ImGui::TableSetColumnIndex(1);
            drawSummaryCard("项目统计", {
                {"地图索引", mapIndexBuffer.data()},
                {"可放置对象", objectAssetBuffer.data()},
                {"对象分类", categoryBuffer.data()},
                {"场景对象", propBuffer.data()},
                {"出生点", spawnBuffer.data()},
                {"相机", cameraBuffer.data()},
            });
            ImGui::EndTable();
        }
    }

    void drawMapEditorPlacementAndToolSection(const RenderFrame& renderFrame, const EditorAssetSelectionState& assetState) {
        ImGui::SeparatorText("编辑工具");
        if (ImGui::BeginTable("editor-top-toolbar", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            static constexpr std::array<const char*, 3> kToolLabels{
                "选择",
                "放置",
                "擦除",
            };
            for (std::size_t toolIndex = 0; toolIndex < kToolLabels.size(); ++toolIndex) {
                if (toolIndex > 0) {
                    ImGui::SameLine();
                }
                if (ImGui::Selectable(
                        kToolLabels[toolIndex],
                        renderFrame.editorToolLabel == kToolLabels[toolIndex],
                        0,
                        ImVec2(72.0f, 0.0f))) {
                    queueUiAction(UiActionType::SelectMapEditorTool, static_cast<std::int32_t>(toolIndex));
                }
            }

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Selectable("自由镜头", !renderFrame.editorIsOrthoView, 0, ImVec2(92.0f, 0.0f)) &&
                renderFrame.editorIsOrthoView) {
                queueUiAction(UiActionType::ToggleMapEditorProjection);
            }
            ImGui::SameLine();
            if (ImGui::Selectable("2.5D 正交", renderFrame.editorIsOrthoView, 0, ImVec2(92.0f, 0.0f)) &&
                !renderFrame.editorIsOrthoView) {
                queueUiAction(UiActionType::ToggleMapEditorProjection);
            }
            ImGui::EndTable();
        }
        ImGui::TextWrapped(
            renderFrame.editorIsOrthoView
                ? "2.5D 正交: WASD 平移，滚轮缩放。"
                : "自由镜头: 右键环视，WASD 飞行。");

        ImGui::SeparatorText("可视化");
        bool showMeshOutline = renderFrame.editorShowMeshOutline;
        if (ImGui::Checkbox("真实轮廓", &showMeshOutline)) {
            queueUiAction(UiActionType::ToggleMapEditorMeshOutline, showMeshOutline ? 1 : 0);
        }
        ImGui::SameLine();
        bool showCollisionOutline = renderFrame.editorShowCollisionOutline;
        if (ImGui::Checkbox("碰撞箱轮廓", &showCollisionOutline)) {
            queueUiAction(UiActionType::ToggleMapEditorCollisionOutline, showCollisionOutline ? 1 : 0);
        }
        ImGui::SameLine();
        bool showBoundingBox = renderFrame.editorShowBoundingBox;
        if (ImGui::Checkbox("最小包裹箱", &showBoundingBox)) {
            queueUiAction(UiActionType::ToggleMapEditorBoundingBox, showBoundingBox ? 1 : 0);
        }

        if (renderFrame.editorToolLabel == "放置") {
            drawMapEditorPlacementSection(renderFrame, assetState);
        } else if (renderFrame.editorToolLabel == "选择") {
            ImGui::SeparatorText("选择提示");
            if (renderFrame.hoveredEditorPropIndex >= 0) {
                ImGui::TextWrapped("当前悬停: %s", renderFrame.selectedEditorPropLabel.c_str());
            } else if (renderFrame.hoveredEditorSpawnIndex >= 0) {
                ImGui::TextWrapped("当前悬停: %s", renderFrame.editorCellSpawnLabel.c_str());
            } else {
                ImGui::TextWrapped("把鼠标指向对象即可自动选择。");
            }
        } else {
            ImGui::SeparatorText("擦除提示");
            if (renderFrame.eraseEditorPropIndex >= 0 || renderFrame.eraseEditorSpawnIndex >= 0) {
                ImGui::TextColored(ImVec4(0.98f, 0.56f, 0.38f, 1.00f), "当前红色高亮对象会在点击时被删除。");
            } else {
                ImGui::TextWrapped("把鼠标移到对象上会出现删除预警高亮。");
            }
        }
    }

    void drawMapEditorContextSection(const RenderFrame& renderFrame) {
        ImGui::SeparatorText("场景上下文");
        if (ImGui::BeginTable("editor-context", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            auto infoRow = [](const char* label, const char* value) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", value);
            };
            std::array<char, 128> targetBuffer{};
            std::snprintf(targetBuffer.data(), targetBuffer.size(), "%.2f, %.2f, %.2f",
                renderFrame.editorTargetPosition.x,
                renderFrame.editorTargetPosition.y,
                renderFrame.editorTargetPosition.z);
            std::array<char, 128> cameraBuffer{};
            std::snprintf(cameraBuffer.data(), cameraBuffer.size(), "%.1f, %.1f, %.1f",
                renderFrame.cameraPosition.x,
                renderFrame.cameraPosition.y,
                renderFrame.cameraPosition.z);
            std::array<char, 64> propCountBuffer{};
            std::snprintf(propCountBuffer.data(), propCountBuffer.size(), "%d", renderFrame.editorPropCount);
            std::array<char, 64> spawnCountBuffer{};
            std::snprintf(spawnCountBuffer.data(), spawnCountBuffer.size(), "%d", renderFrame.editorSpawnCount);
            infoRow("目标", targetBuffer.data());
            infoRow("相机", cameraBuffer.data());
            infoRow("地面", renderFrame.editorCellFloorLabel.c_str());
            infoRow("掩体", renderFrame.editorCellCoverLabel.c_str());
            infoRow("道具", renderFrame.editorCellPropLabel.c_str());
            infoRow("出生点", renderFrame.editorCellSpawnLabel.c_str());
            infoRow("对象总数", propCountBuffer.data());
            infoRow("出生点总数", spawnCountBuffer.data());
            ImGui::EndTable();
        }

        const float buttonWidth = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
        if (ImGui::Button("应用当前工具", ImVec2(buttonWidth, 0.0f))) {
            if (renderFrame.editorToolLabel == "擦除") {
                queueUiAction(UiActionType::EraseMapEditorCell);
            } else if (renderFrame.editorToolLabel != "选择") {
                queueUiAction(UiActionType::ApplyMapEditorTool);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("保存地图副本", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::SaveEditorMap);
        }
    }

    std::string buildManagedObjectDraftFingerprint(const RenderFrame& renderFrame) const {
        std::ostringstream out;
        out << renderFrame.editorManagedObjectAssetId << '|'
            << renderFrame.editorManagedObjectAssetLabel << '|'
            << renderFrame.editorManagedObjectAssetCategory << '|'
            << renderFrame.editorManagedObjectModelPath << '|'
            << renderFrame.editorManagedObjectMaterialPath << '|'
            << renderFrame.editorManagedObjectTags << '|'
            << renderFrame.editorManagedObjectCollisionHalfExtents.x << ','
            << renderFrame.editorManagedObjectCollisionHalfExtents.y << ','
            << renderFrame.editorManagedObjectCollisionHalfExtents.z << '|'
            << renderFrame.editorManagedObjectCollisionCenterOffset.x << ','
            << renderFrame.editorManagedObjectCollisionCenterOffset.y << ','
            << renderFrame.editorManagedObjectCollisionCenterOffset.z << '|'
            << renderFrame.editorManagedObjectPreviewColor.x << ','
            << renderFrame.editorManagedObjectPreviewColor.y << ','
            << renderFrame.editorManagedObjectPreviewColor.z << '|'
            << renderFrame.editorManagedObjectPlacementKind << '|'
            << renderFrame.editorManagedObjectCylindrical << '|'
            << renderFrame.editorManagedObjectEditorVisible;
        return out.str();
    }

    void syncManagedObjectDraft(const RenderFrame& renderFrame) {
        if (!renderFrame.editorHasManagedObjectAsset) {
            managedObjectDraftFingerprint_.clear();
            return;
        }

        const std::string fingerprint = buildManagedObjectDraftFingerprint(renderFrame);
        if (fingerprint == managedObjectDraftFingerprint_) {
            return;
        }

        managedObjectDraftFingerprint_ = fingerprint;
        copyStringToBuffer(managedObjectIdDraft_, renderFrame.editorManagedObjectAssetId);
        copyStringToBuffer(managedObjectLabelDraft_, renderFrame.editorManagedObjectAssetLabel);
        copyStringToBuffer(managedObjectCategoryDraft_, renderFrame.editorManagedObjectAssetCategory);
        copyStringToBuffer(managedObjectModelPathDraft_, renderFrame.editorManagedObjectModelPath);
        copyStringToBuffer(managedObjectMaterialPathDraft_, renderFrame.editorManagedObjectMaterialPath);
        copyStringToBuffer(managedObjectTagsDraft_, renderFrame.editorManagedObjectTags);
        managedObjectPlacementKindDraft_ = renderFrame.editorManagedObjectPlacementKind;
        managedObjectCylindricalDraft_ = renderFrame.editorManagedObjectCylindrical;
        managedObjectEditorVisibleDraft_ = renderFrame.editorManagedObjectEditorVisible;
        managedObjectCollisionHalfExtentsDraft_[0] = renderFrame.editorManagedObjectCollisionHalfExtents.x;
        managedObjectCollisionHalfExtentsDraft_[1] = renderFrame.editorManagedObjectCollisionHalfExtents.y;
        managedObjectCollisionHalfExtentsDraft_[2] = renderFrame.editorManagedObjectCollisionHalfExtents.z;
        managedObjectCollisionCenterOffsetDraft_[0] = renderFrame.editorManagedObjectCollisionCenterOffset.x;
        managedObjectCollisionCenterOffsetDraft_[1] = renderFrame.editorManagedObjectCollisionCenterOffset.y;
        managedObjectCollisionCenterOffsetDraft_[2] = renderFrame.editorManagedObjectCollisionCenterOffset.z;
        managedObjectPreviewColorDraft_[0] = renderFrame.editorManagedObjectPreviewColor.x;
        managedObjectPreviewColorDraft_[1] = renderFrame.editorManagedObjectPreviewColor.y;
        managedObjectPreviewColorDraft_[2] = renderFrame.editorManagedObjectPreviewColor.z;
    }

    void drawManagedObjectAssetSection(const RenderFrame& renderFrame) {
        syncManagedObjectDraft(renderFrame);

        if (renderFrame.editorObjectAssets == nullptr || renderFrame.editorObjectAssets->empty()) {
            ImGui::TextWrapped("当前没有任何对象资产。点击下方按钮创建第一个对象。");
            if (ImGui::Button("新建对象副本")) {
                queueUiAction(UiActionType::CreateManagedObjectAsset);
            }
            return;
        }

        const auto& objectAssets = *renderFrame.editorObjectAssets;
        const content::ObjectAssetDefinition* selectedAsset = nullptr;
        if (renderFrame.editorManagedObjectAssetIndex < objectAssets.size()) {
            selectedAsset = &objectAssets[renderFrame.editorManagedObjectAssetIndex];
        }

        auto drawAssetListPane = [&]() {
            ImGui::Text("对象资产 %zu", objectAssets.size());
            ImGui::InputTextWithHint(
                "##managed-object-filter", "过滤对象名称、分类或 id...", managedObjectFilter_.data(), managedObjectFilter_.size());
            ImGui::Separator();
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const float listWidth = ImGui::GetContentRegionAvail().x;
            const bool stackButtons = listWidth < 280.0f;
            const float listButtonWidth = stackButtons ? listWidth : (listWidth - gap) * 0.5f;
            if (ImGui::Button("新建副本", ImVec2(listButtonWidth, 0.0f))) {
                queueUiAction(UiActionType::CreateManagedObjectAsset);
            }
            if (!stackButtons) {
                ImGui::SameLine();
            }
            const bool canDelete = renderFrame.editorManagedObjectActiveMapRefCount == 0 &&
                renderFrame.editorManagedObjectStoredMapRefCount == 0;
            if (!canDelete) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("删除", ImVec2(listButtonWidth, 0.0f))) {
                queueUiAction(UiActionType::DeleteManagedObjectAsset);
            }
            if (!canDelete) {
                ImGui::EndDisabled();
            }
            ImGui::Separator();

            const std::string filter = lowerAsciiCopy(managedObjectFilter_.data());
            int filteredCount = 0;
            for (const auto& asset : objectAssets) {
                const std::string haystack = lowerAsciiCopy(
                    asset.label + " " + asset.id + " " + asset.category + " " +
                    joinTags(asset.tags) + " " + asset.modelPath.generic_string() + " " +
                    asset.materialPath.generic_string());
                if (filter.empty() || haystack.find(filter) != std::string::npos) {
                    ++filteredCount;
                }
            }
            if (!filter.empty()) {
                ImGui::TextDisabled("筛选结果: %d", filteredCount);
            }

            struct ManagedObjectCategoryGroup {
                std::string category;
                std::vector<std::size_t> indices;
            };

            std::vector<ManagedObjectCategoryGroup> groups;
            for (std::size_t index = 0; index < objectAssets.size(); ++index) {
                const auto& asset = objectAssets[index];
                const std::string haystack = lowerAsciiCopy(
                    asset.label + " " + asset.id + " " + asset.category + " " +
                    joinTags(asset.tags) + " " + asset.modelPath.generic_string() + " " +
                    asset.materialPath.generic_string());
                if (!filter.empty() && haystack.find(filter) == std::string::npos) {
                    continue;
                }
                if (groups.empty() || groups.back().category != asset.category) {
                    groups.push_back(ManagedObjectCategoryGroup{
                        .category = asset.category.empty() ? "未分类" : asset.category,
                        .indices = {},
                    });
                }
                groups.back().indices.push_back(index);
            }

            if (groups.empty()) {
                ImGui::TextDisabled("没有匹配的对象资产。");
                return;
            }

            for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
                const auto& group = groups[groupIndex];
                const std::string headerLabel = group.category + "##managed-object-category-" + std::to_string(groupIndex);
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (!ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    continue;
                }

                for (const std::size_t index : group.indices) {
                    const auto& asset = objectAssets[index];
                    ImGui::PushID(static_cast<int>(index));
                    const bool selected = index == renderFrame.editorManagedObjectAssetIndex;
                    const float lineHeight = ImGui::GetTextLineHeight();
                    const float rowHeight = lineHeight * 2.0f + 8.0f;
                    const float rowWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                    if (ImGui::Selectable("##managed-object-row", selected, 0, ImVec2(rowWidth, rowHeight))) {
                        queueUiAction(UiActionType::SelectManagedObjectAsset, static_cast<std::int32_t>(index));
                    }

                    const bool hovered = ImGui::IsItemHovered();
                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    const ImVec2 itemMax = ImGui::GetItemRectMax();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const float rounding = 6.0f;
                    const ImU32 borderColor = ImGui::GetColorU32(selected ? ImVec4(0.42f, 0.72f, 0.96f, 0.95f)
                                                                         : ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
                    const ImU32 fillColor = ImGui::GetColorU32(selected ? ImVec4(0.16f, 0.28f, 0.38f, 0.90f)
                                                                       : (hovered ? ImVec4(1.0f, 1.0f, 1.0f, 0.05f)
                                                                                  : ImVec4(1.0f, 1.0f, 1.0f, 0.02f)));
                    const ImU32 primaryTextColor = ImGui::GetColorU32(selected ? ImVec4(0.96f, 0.98f, 1.0f, 1.0f)
                                                                               : ImGui::GetStyleColorVec4(ImGuiCol_Text));
                    const ImU32 secondaryTextColor = ImGui::GetColorU32(selected ? ImVec4(0.78f, 0.87f, 0.95f, 1.0f)
                                                                                 : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    const float textLeft = itemMin.x + 10.0f;
                    const float labelTop = itemMin.y + 3.0f;
                    const float metaTop = labelTop + lineHeight + 2.0f;
                    std::string metaLine = asset.category.empty() ? asset.id : asset.category + " | " + asset.id;
                    const std::string tags = joinTags(asset.tags);
                    if (!tags.empty()) {
                        metaLine += " | ";
                        metaLine += tags;
                    }

                    drawList->AddRectFilled(itemMin, itemMax, fillColor, rounding);
                    drawList->AddRect(itemMin, itemMax, borderColor, rounding, 0, selected ? 1.6f : 1.0f);
                    drawList->PushClipRect(itemMin, itemMax, true);
                    drawList->AddText(ImVec2(textLeft, labelTop), primaryTextColor, asset.label.c_str());
                    drawList->AddText(ImVec2(textLeft, metaTop), secondaryTextColor, metaLine.c_str());
                    drawList->PopClipRect();
                    ImGui::PopID();
                }
            }
        };

        auto drawAssetDetailPane = [&]() {
            if (selectedAsset != nullptr) {
                const ImTextureID preview = cachedImGuiPreviewTextureForObject(
                    selectedAsset->thumbnailPath,
                    selectedAsset->modelPath);
                drawMapEditorAssetPreviewCard(
                    "对象预览",
                    preview,
                    selectedAsset->category,
                    joinTags(selectedAsset->tags),
                    selectedAsset->modelPath.generic_string(),
                    selectedAsset->materialPath.generic_string());
                ImGui::TextWrapped("修改字段后离开输入框会自动保存到对象清单。");
                ImGui::Text("地图引用: 当前 %d / 磁盘 %d",
                    renderFrame.editorManagedObjectActiveMapRefCount,
                    renderFrame.editorManagedObjectStoredMapRefCount);
                ImGui::Separator();
            }

            if (!renderFrame.editorHasManagedObjectAsset) {
                ImGui::TextWrapped("请选择左侧对象资产。");
                return;
            }

            auto beginFormTable = [](const char* tableId) {
                if (!ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
                    return false;
                }
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 88.0f);
                ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
                return true;
            };
            auto nextRow = [](const char* label) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
            };

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("基础信息", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (beginFormTable("managed-object-form-basic")) {
                    nextRow("对象 ID");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##managed-object-id", managedObjectIdDraft_.data(), managedObjectIdDraft_.size());
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiTextAction(UiActionType::SetManagedObjectAssetId, managedObjectIdDraft_.data());
                    }

                    nextRow("显示名称");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##managed-object-label", managedObjectLabelDraft_.data(), managedObjectLabelDraft_.size());
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiTextAction(UiActionType::SetManagedObjectAssetLabel, managedObjectLabelDraft_.data());
                    }

                    nextRow("分类");
                    const std::string currentCategory = managedObjectCategoryDraft_.data();
                    const char* previewCategory = currentCategory.empty() ? kManagedObjectCategories.front() : currentCategory.c_str();
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo("##managed-object-category", previewCategory)) {
                        bool matchedPreset = false;
                        for (const char* option : kManagedObjectCategories) {
                            const bool isSelected = currentCategory == option;
                            matchedPreset = matchedPreset || isSelected;
                            if (ImGui::Selectable(option, isSelected)) {
                                copyStringToBuffer(managedObjectCategoryDraft_, option);
                                queueUiTextAction(UiActionType::SetManagedObjectAssetCategory, option);
                            }
                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        if (!currentCategory.empty() && !matchedPreset) {
                            ImGui::Separator();
                            if (ImGui::Selectable(currentCategory.c_str(), true)) {
                                copyStringToBuffer(managedObjectCategoryDraft_, currentCategory);
                                queueUiTextAction(UiActionType::SetManagedObjectAssetCategory, currentCategory);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    nextRow("标签");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputTextWithHint("##managed-object-tags", "crate, cover, custom", managedObjectTagsDraft_.data(), managedObjectTagsDraft_.size());
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiTextAction(UiActionType::SetManagedObjectAssetTags, managedObjectTagsDraft_.data());
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("资源路径", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (beginFormTable("managed-object-form-paths")) {
                    nextRow("模型路径");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##managed-object-model-path", managedObjectModelPathDraft_.data(), managedObjectModelPathDraft_.size());
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiTextAction(UiActionType::SetManagedObjectAssetModelPath, managedObjectModelPathDraft_.data());
                    }

                    nextRow("材质路径");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##managed-object-material-path", managedObjectMaterialPathDraft_.data(), managedObjectMaterialPathDraft_.size());
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiTextAction(UiActionType::SetManagedObjectAssetMaterialPath, managedObjectMaterialPathDraft_.data());
                    }
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("碰撞与显示", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (beginFormTable("managed-object-form-collision")) {
                    nextRow("碰撞半径");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::DragFloat3("##managed-object-half-extents", managedObjectCollisionHalfExtentsDraft_, 0.01f, 0.01f, 64.0f, "%.2f");
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiVec3Action(UiActionType::SetManagedObjectAssetCollisionHalfExtents,
                            util::Vec3{
                                managedObjectCollisionHalfExtentsDraft_[0],
                                managedObjectCollisionHalfExtentsDraft_[1],
                                managedObjectCollisionHalfExtentsDraft_[2]});
                    }

                    nextRow("中心偏移");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::DragFloat3("##managed-object-center-offset", managedObjectCollisionCenterOffsetDraft_, 0.01f, -64.0f, 64.0f, "%.2f");
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiVec3Action(UiActionType::SetManagedObjectAssetCollisionCenterOffset,
                            util::Vec3{
                                managedObjectCollisionCenterOffsetDraft_[0],
                                managedObjectCollisionCenterOffsetDraft_[1],
                                managedObjectCollisionCenterOffsetDraft_[2]});
                    }

                    nextRow("预览颜色");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::SliderFloat3("##managed-object-preview-color", managedObjectPreviewColorDraft_, 0.0f, 255.0f, "%.0f");
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        queueUiVec3Action(UiActionType::SetManagedObjectAssetPreviewColor,
                            util::Vec3{
                                managedObjectPreviewColorDraft_[0],
                                managedObjectPreviewColorDraft_[1],
                                managedObjectPreviewColorDraft_[2]});
                    }

                    nextRow("碰撞形状");
                    if (ImGui::Checkbox("圆柱足迹##managed-object-cyl", &managedObjectCylindricalDraft_)) {
                        queueUiAction(UiActionType::ToggleManagedObjectAssetCylindrical, managedObjectCylindricalDraft_ ? 1 : 0);
                    }

                    nextRow("编辑器可见");
                    if (ImGui::Checkbox("在放置列表显示##managed-object-visible", &managedObjectEditorVisibleDraft_)) {
                        queueUiAction(UiActionType::ToggleManagedObjectAssetEditorVisible, managedObjectEditorVisibleDraft_ ? 1 : 0);
                    }

                    nextRow("地图引用");
                    ImGui::Text("当前地图 %d 个，磁盘地图 %d 个",
                        renderFrame.editorManagedObjectActiveMapRefCount,
                        renderFrame.editorManagedObjectStoredMapRefCount);
                    ImGui::EndTable();
                }
            }
        };

        const bool compactLayout = ImGui::GetContentRegionAvail().x < 860.0f;
        if (compactLayout) {
            ImGui::BeginChild("managed-object-list", ImVec2(0.0f, 246.0f), true);
            drawAssetListPane();
            ImGui::EndChild();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::BeginChild("managed-object-detail", ImVec2(0.0f, 0.0f), false);
            drawAssetDetailPane();
            ImGui::EndChild();
            return;
        }

        if (ImGui::BeginTable("managed-object-layout", 2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("managed-object-list", ImVec2(0.0f, 0.0f), true);
            drawAssetListPane();
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            ImGui::BeginChild("managed-object-detail", ImVec2(0.0f, 0.0f), false);
            drawAssetDetailPane();
            ImGui::EndChild();
            ImGui::EndTable();
        }
    }

    void buildMapEditorWindow(const RenderFrame& renderFrame) {
        const EditorAssetSelectionState assetState = resolveEditorAssetSelectionState(renderFrame);
        const ImGuiIO& io = ImGui::GetIO();
        const float minSidebarWidth = 360.0f;
        const float maxSidebarWidth = std::max(minSidebarWidth, io.DisplaySize.x * 0.60f);
        if (editorSidebarWidthDraft_ <= 0.0f || (!editorSidebarWidthDragging_ &&
            std::abs(editorSidebarWidthDraft_ - renderFrame.editorSidebarWidth) > 0.5f)) {
            editorSidebarWidthDraft_ = std::clamp(renderFrame.editorSidebarWidth, minSidebarWidth, maxSidebarWidth);
        }
        const float sidebarWidth = std::clamp(editorSidebarWidthDraft_, minSidebarWidth, maxSidebarWidth);
        const float topBarHeight = 42.0f;
        const float bottomBarHeight = 26.0f;
        const float sidebarHeight = std::max(1.0f, io.DisplaySize.y - topBarHeight - bottomBarHeight);
        const float sidebarX = std::max(0.0f, io.DisplaySize.x - sidebarWidth);
        const float splitterWidth = 6.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 5.0f));
        constexpr ImGuiWindowFlags kEditorWindowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;
        constexpr ImGuiWindowFlags kEditorBarWindowFlags =
            kEditorWindowFlags |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoResize;
        constexpr ImGuiWindowFlags kEditorSidebarFlags =
            kEditorWindowFlags |
            ImGuiWindowFlags_NoResize;
        constexpr ImGuiWindowFlags kEditorSplitterFlags =
            kEditorWindowFlags |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, io.DisplaySize.x), topBarHeight), ImGuiCond_Always);
        if (ImGui::Begin("地图编辑器顶部工具条", nullptr, kEditorBarWindowFlags)) {
            drawMapEditorTopBarContent(renderFrame);
        }
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 2.0f));
        ImGui::SetNextWindowPos(ImVec2(0.0f, std::max(0.0f, io.DisplaySize.y - bottomBarHeight)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, io.DisplaySize.x), bottomBarHeight), ImGuiCond_Always);
        if (ImGui::Begin("地图编辑器底部状态条", nullptr, kEditorBarWindowFlags)) {
            drawMapEditorBottomStatusContent(renderFrame);
        }
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::SetNextWindowPos(ImVec2(sidebarX - splitterWidth, topBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(splitterWidth, sidebarHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("地图编辑器侧栏分隔条", nullptr, kEditorSplitterFlags)) {
            ImGui::InvisibleButton("##editor-sidebar-resize", ImVec2(splitterWidth, sidebarHeight));
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if (ImGui::IsItemActive()) {
                editorSidebarWidthDragging_ = true;
                editorSidebarResizeDirty_ = true;
                editorSidebarWidthDraft_ = std::clamp(io.DisplaySize.x - io.MousePos.x, minSidebarWidth, maxSidebarWidth);
            } else if (editorSidebarWidthDragging_) {
                editorSidebarWidthDragging_ = false;
                if (editorSidebarResizeDirty_) {
                    queueUiAction(UiActionType::SetEditorSidebarWidth, static_cast<std::int32_t>(std::lround(editorSidebarWidthDraft_)));
                    editorSidebarResizeDirty_ = false;
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::SetNextWindowPos(ImVec2(sidebarX, topBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(sidebarWidth, sidebarHeight),
            ImGuiCond_Always);
        if (ImGui::Begin("地图编辑器控制台", nullptr, kEditorSidebarFlags)) {
            if (ImGui::BeginChild("editor-sidebar-body", ImVec2(0.0f, 0.0f), false)) {
                switch (selectedMapEditorTopTab_) {
                    case MapEditorTopTab::Scene:
                    {
                        drawMapEditorSceneCollectionSection(renderFrame);
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                        drawMapEditorObjectEditingSection(renderFrame, assetState);
                        break;
                    }
                    case MapEditorTopTab::MapInfo:
                        drawMapEditorMapInfoSection(renderFrame);
                        break;
                    case MapEditorTopTab::Assets:
                        drawManagedObjectAssetSection(renderFrame);
                        break;
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
        const ImVec2 floatingToolbarPos{12.0f, topBarHeight + 12.0f};
        drawFloatingMapEditorToolWindow(renderFrame, floatingToolbarPos);
        const ImVec2 floatingViewPos{
            std::max(12.0f, sidebarX - splitterWidth - 138.0f),
            topBarHeight + 12.0f,
        };
        drawFloatingMapEditorViewWindow(renderFrame, floatingViewPos);
        ImGui::PopStyleVar(4);
    }
