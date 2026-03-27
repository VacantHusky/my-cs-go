    void drawMapEditorInspectorSection(const RenderFrame& renderFrame) {
        ImGui::SeparatorText("对象参数");
        if (renderFrame.hasSelectedEditorProp) {
            ImGui::Text("名称: %s", renderFrame.selectedEditorPropLabel.c_str());
            ImGui::TextWrapped("对象 ID: %s", renderFrame.selectedEditorPropAssetId.c_str());
            ImGui::TextWrapped("分类: %s", renderFrame.selectedEditorPropCategoryLabel.c_str());
            ImGui::TextWrapped("模型: %s", renderFrame.selectedEditorPropModelLabel.c_str());
            ImGui::TextWrapped("材质: %s", renderFrame.selectedEditorPropMaterialLabel.c_str());
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

    void drawMapEditorHeaderBar(const RenderFrame& renderFrame) {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        ImGui::TextWrapped("当前地图: %s", renderFrame.editorMapFileLabel.c_str());
        ImGui::TextWrapped("工具: %s | 视图: %s", renderFrame.editorToolLabel.c_str(), renderFrame.editorViewModeLabel.c_str());
        ImGui::TextWrapped("状态: %s", renderFrame.editorStatusLabel.c_str());

        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float buttonWidth = std::max(64.0f, (availableWidth - gap * 5.0f) / 6.0f);

        if (ImGui::Button("保存", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::SaveEditorMap);
        }
        ImGui::SameLine();
        if (!renderFrame.editorUndoAvailable) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("撤销", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::UndoMapEditorChange);
        }
        if (!renderFrame.editorUndoAvailable) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("上一张", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::CycleEditorMap, -1);
        }
        ImGui::SameLine();
        if (ImGui::Button("下一张", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::CycleEditorMap, 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("新建", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::CreateEditorMap);
        }
        ImGui::SameLine();
        if (ImGui::Button("返回", ImVec2(buttonWidth, 0.0f))) {
            queueUiAction(UiActionType::ReturnToMainMenu);
        }
    }

    void drawMapEditorMapInfoSection(const RenderFrame& renderFrame) {
        ImGui::SeparatorText("当前地图");
        if (ImGui::BeginTable("editor-map-info", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            auto infoRow = [](const char* label, const std::string& value) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", value.c_str());
            };

            infoRow("地图文件", renderFrame.editorMapFileLabel);
            infoRow("对象清单", renderFrame.editorAssetManifestLabel);
            infoRow("当前工具", renderFrame.editorToolLabel);
            infoRow("当前视图", renderFrame.editorViewModeLabel);
            infoRow("编辑状态", renderFrame.editorStatusLabel);
            ImGui::EndTable();
        }

        ImGui::SeparatorText("项目统计");
        if (ImGui::BeginTable("editor-map-stats", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("地图索引: %zu / %zu",
                renderFrame.editorMapIndex + 1,
                std::max<std::size_t>(renderFrame.editorMapCount, 1));
            ImGui::Text("可放置对象: %d", renderFrame.editorObjectAssetCount);
            ImGui::Text("对象分类: %d", renderFrame.editorObjectCategoryCount);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("场景对象: %d", renderFrame.editorPropCount);
            ImGui::Text("出生点: %d", renderFrame.editorSpawnCount);
            ImGui::Text("相机: %.1f, %.1f, %.1f",
                renderFrame.cameraPosition.x,
                renderFrame.cameraPosition.y,
                renderFrame.cameraPosition.z);
            ImGui::EndTable();
        }
    }

    void drawMapEditorPlacementAndToolSection(const RenderFrame& renderFrame, const EditorAssetSelectionState& assetState) {
        ImGui::SeparatorText("编辑工具");
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
                    ImVec2(92.0f, 0.0f))) {
                queueUiAction(UiActionType::SelectMapEditorTool, static_cast<std::int32_t>(toolIndex));
            }
        }

        ImGui::SeparatorText("视图");
        if (ImGui::Selectable("自由镜头", !renderFrame.editorIsOrthoView, 0, ImVec2(120.0f, 0.0f)) &&
            renderFrame.editorIsOrthoView) {
            queueUiAction(UiActionType::ToggleMapEditorProjection);
        }
        ImGui::SameLine();
        if (ImGui::Selectable("2.5D 正交", renderFrame.editorIsOrthoView, 0, ImVec2(120.0f, 0.0f)) &&
            !renderFrame.editorIsOrthoView) {
            queueUiAction(UiActionType::ToggleMapEditorProjection);
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
        bool showCollisionOutline = renderFrame.editorShowCollisionOutline;
        if (ImGui::Checkbox("碰撞箱轮廓", &showCollisionOutline)) {
            queueUiAction(UiActionType::ToggleMapEditorCollisionOutline, showCollisionOutline ? 1 : 0);
        }
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
        if (ImGui::BeginTable("editor-context", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("目标  %.2f, %.2f, %.2f",
                renderFrame.editorTargetPosition.x,
                renderFrame.editorTargetPosition.y,
                renderFrame.editorTargetPosition.z);
            ImGui::Text("相机  %.1f, %.1f, %.1f",
                renderFrame.cameraPosition.x,
                renderFrame.cameraPosition.y,
                renderFrame.cameraPosition.z);
            ImGui::Text("地面: %s", renderFrame.editorCellFloorLabel.c_str());
            ImGui::Text("掩体: %s", renderFrame.editorCellCoverLabel.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("道具: %s", renderFrame.editorCellPropLabel.c_str());
            ImGui::Text("出生点: %s", renderFrame.editorCellSpawnLabel.c_str());
            ImGui::Text("对象总数: %d", renderFrame.editorPropCount);
            ImGui::Text("出生点总数: %d", renderFrame.editorSpawnCount);
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
            ImGui::Text("对象资产 %d", renderFrame.editorObjectAssetCount);
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
            std::string currentCategory;
            for (std::size_t index = 0; index < objectAssets.size(); ++index) {
                const auto& asset = objectAssets[index];
                const std::string haystack = lowerAsciiCopy(
                    asset.label + " " + asset.id + " " + asset.category + " " +
                    joinTags(asset.tags) + " " + asset.modelPath.generic_string() + " " +
                    asset.materialPath.generic_string());
                if (!filter.empty() && haystack.find(filter) == std::string::npos) {
                    continue;
                }
                if (asset.category != currentCategory) {
                    currentCategory = asset.category;
                    ImGui::SeparatorText(currentCategory.c_str());
                }

                ImGui::PushID(static_cast<int>(index));
                const bool selected = index == renderFrame.editorManagedObjectAssetIndex;
                if (ImGui::Selectable(asset.label.c_str(), selected, 0, ImVec2(-1.0f, 0.0f))) {
                    queueUiAction(UiActionType::SelectManagedObjectAsset, static_cast<std::int32_t>(index));
                }
                ImGui::TextDisabled("%s", asset.id.c_str());
                ImGui::PopID();
            }
        };

        auto drawAssetDetailPane = [&]() {
            if (selectedAsset != nullptr) {
                const ImTextureID preview = cachedImGuiPreviewTexture(selectedAsset->thumbnailPath);
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

            if (ImGui::BeginTable("managed-object-form", 2,
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 102.0f);
                ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

                auto nextRow = [](const char* label) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::TableSetColumnIndex(1);
                };

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
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("##managed-object-category", managedObjectCategoryDraft_.data(), managedObjectCategoryDraft_.size());
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    queueUiTextAction(UiActionType::SetManagedObjectAssetCategory, managedObjectCategoryDraft_.data());
                }

                nextRow("类型");
                if (ImGui::RadioButton("道具##managed_object_prop", managedObjectPlacementKindDraft_ == 0)) {
                    managedObjectPlacementKindDraft_ = 0;
                    queueUiAction(UiActionType::SetManagedObjectAssetPlacementKind, 0);
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("墙体##managed_object_wall", managedObjectPlacementKindDraft_ == 1)) {
                    managedObjectPlacementKindDraft_ = 1;
                    queueUiAction(UiActionType::SetManagedObjectAssetPlacementKind, 1);
                }

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

                nextRow("标签");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##managed-object-tags", "crate, cover, custom", managedObjectTagsDraft_.data(), managedObjectTagsDraft_.size());
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    queueUiTextAction(UiActionType::SetManagedObjectAssetTags, managedObjectTagsDraft_.data());
                }

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
                ImGui::Text("当前地图 %d  个，磁盘地图 %d  个",
                    renderFrame.editorManagedObjectActiveMapRefCount,
                    renderFrame.editorManagedObjectStoredMapRefCount);

                ImGui::EndTable();
            }
        };

        const bool compactLayout = ImGui::GetContentRegionAvail().x < 860.0f;
        if (compactLayout) {
            ImGui::BeginChild("managed-object-list", ImVec2(0.0f, 250.0f), true);
            drawAssetListPane();
            ImGui::EndChild();
            ImGui::Spacing();
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
        const float sidebarWidth = std::min(680.0f, std::max(520.0f, io.DisplaySize.x * 0.34f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sidebarWidth, std::max(1.0f, io.DisplaySize.y)), ImGuiCond_Always);
        constexpr ImGuiWindowFlags kEditorWindowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("地图编辑器控制台", nullptr, kEditorWindowFlags)) {
            ImGui::TextUnformatted("地图编辑器");
            ImGui::TextDisabled("Scene Authoring Workspace");
            drawMapEditorHeaderBar(renderFrame);
            ImGui::Separator();
            if (ImGui::BeginChild("editor-sidebar-body", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (ImGui::BeginTabBar("editor-main-tabs")) {
                    if (ImGui::BeginTabItem("场景编辑")) {
                        drawMapEditorPlacementAndToolSection(renderFrame, assetState);
                        ImGui::Spacing();
                        drawMapEditorContextSection(renderFrame);
                        ImGui::Spacing();
                        drawMapEditorInspectorSection(renderFrame);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("地图信息")) {
                        drawMapEditorMapInfoSection(renderFrame);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("资产管理")) {
                        drawManagedObjectAssetSection(renderFrame);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
