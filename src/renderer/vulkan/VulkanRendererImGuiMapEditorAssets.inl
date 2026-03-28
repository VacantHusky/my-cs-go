    struct EditorAssetSelectionState {
        const std::vector<content::ObjectAssetDefinition>* objectAssets = nullptr;
        const content::ObjectAssetDefinition* selectedObjectAsset = nullptr;
    };

    bool editorObjectMatchesPlacement(const content::ObjectAssetDefinition& object,
                                      const RenderFrame& renderFrame) const {
        if (!object.editorVisible) {
            return false;
        }
        if (renderFrame.editorPlacementKindLabel == "盒体墙") {
            return object.placementKind == content::ObjectPlacementKind::Wall;
        }
        if (renderFrame.editorPlacementKindLabel == "道具") {
            return object.placementKind == content::ObjectPlacementKind::Prop;
        }
        return false;
    }

    EditorAssetSelectionState resolveEditorAssetSelectionState(const RenderFrame& renderFrame) const {
        EditorAssetSelectionState state{};
        state.objectAssets = renderFrame.editorObjectAssets;
        if (state.objectAssets == nullptr || state.objectAssets->empty()) {
            return state;
        }

        const std::size_t clampedIndex =
            std::min(renderFrame.editorSelectedObjectAssetIndex, state.objectAssets->size() - 1);
        if (editorObjectMatchesPlacement((*state.objectAssets)[clampedIndex], renderFrame)) {
            state.selectedObjectAsset = &(*state.objectAssets)[clampedIndex];
            return state;
        }

        for (const auto& object : *state.objectAssets) {
            if (editorObjectMatchesPlacement(object, renderFrame)) {
                state.selectedObjectAsset = &object;
                return state;
            }
        }
        return state;
    }

    void drawMapEditorAssetPreviewCard(const char* title,
                                       const ImTextureID imageId,
                                       const std::string& category,
                                       const std::string& tags,
                                       const std::string& primaryPath,
                                       const std::string& secondaryPath) {
        ImGui::BeginGroup();
        ImGui::TextUnformatted(title);
        const float previewSize = 80.0f;
        if (imageId != 0) {
            ImGui::Image(imageId, ImVec2(previewSize, previewSize));
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("无缩略图", ImVec2(previewSize, previewSize));
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (!category.empty()) {
            ImGui::Text("分类: %s", category.c_str());
        }
        if (!tags.empty()) {
            ImGui::TextWrapped("标签: %s", tags.c_str());
        }
        if (!primaryPath.empty()) {
            ImGui::TextWrapped("%s", primaryPath.c_str());
        }
        if (!secondaryPath.empty()) {
            ImGui::TextWrapped("%s", secondaryPath.c_str());
        }
        ImGui::EndGroup();
        ImGui::EndGroup();
    }

    void drawEditorObjectAssetTooltip(const content::ObjectAssetDefinition& asset) {
        ImGui::BeginTooltip();
        if (ImTextureID imageId = cachedImGuiPreviewTextureForObject(asset.thumbnailPath, asset.modelPath); imageId != 0) {
            ImGui::Image(imageId, ImVec2(112.0f, 112.0f));
        }
        ImGui::TextWrapped("分类: %s", asset.category.c_str());
        const std::string tags = joinTags(asset.tags);
        if (!tags.empty()) {
            ImGui::TextWrapped("标签: %s", tags.c_str());
        }
        if (!asset.modelPath.empty()) {
            ImGui::TextWrapped("模型: %s", asset.modelPath.generic_string().c_str());
        }
        if (!asset.materialPath.empty()) {
            ImGui::TextWrapped("材质: %s", asset.materialPath.generic_string().c_str());
        }
        ImGui::Text("碰撞盒: %.2f %.2f %.2f",
            asset.collisionHalfExtents.x * 2.0f,
            asset.collisionHalfExtents.y * 2.0f,
            asset.collisionHalfExtents.z * 2.0f);
        ImGui::EndTooltip();
    }

    void drawEditorObjectAssetCombo(const RenderFrame& renderFrame,
                                    const std::vector<content::ObjectAssetDefinition>& objectAssets) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("对象资产");
        ImGui::TableSetColumnIndex(1);

        const EditorAssetSelectionState state = resolveEditorAssetSelectionState(renderFrame);
        if (state.selectedObjectAsset == nullptr) {
            ImGui::TextDisabled("当前类型没有可放置对象");
            return;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##editor-object-asset", state.selectedObjectAsset->label.c_str())) {
            ImGui::InputTextWithHint(
                "##editor-object-filter", "过滤名称、分类、标签或路径...", editorModelFilter_.data(), editorModelFilter_.size());
            ImGui::Separator();

            const std::string filter = lowerAsciiCopy(editorModelFilter_.data());
            std::string currentCategory;
            for (std::size_t assetIndex = 0; assetIndex < objectAssets.size(); ++assetIndex) {
                const auto& asset = objectAssets[assetIndex];
                if (!editorObjectMatchesPlacement(asset, renderFrame)) {
                    continue;
                }
                const std::string haystack = lowerAsciiCopy(
                    asset.label + " " + asset.category + " " + joinTags(asset.tags) + " " +
                    asset.modelPath.generic_string() + " " + asset.materialPath.generic_string());
                if (!filter.empty() && haystack.find(filter) == std::string::npos) {
                    continue;
                }
                if (asset.category != currentCategory) {
                    currentCategory = asset.category;
                    ImGui::SeparatorText(currentCategory.c_str());
                }

                const bool selected = state.selectedObjectAsset == &asset;
                if (ImGui::Selectable(asset.label.c_str(), selected)) {
                    queueUiAction(UiActionType::SelectEditorObjectAsset, static_cast<std::int32_t>(assetIndex));
                }
                if (ImGui::IsItemHovered()) {
                    drawEditorObjectAssetTooltip(asset);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void drawMapEditorPlacementSection(const RenderFrame& renderFrame, const EditorAssetSelectionState& assetState) {
        ImGui::SeparatorText("放置类型");
        static constexpr std::array<const char*, 4> kPlacementLabels{
            "盒体墙",
            "道具",
            "进攻出生点",
            "防守出生点",
        };
        if (ImGui::BeginTable("editor-placement-grid", 2, ImGuiTableFlags_SizingStretchSame)) {
            for (std::size_t placementIndex = 0; placementIndex < kPlacementLabels.size(); ++placementIndex) {
                if (placementIndex % 2 == 0) {
                    ImGui::TableNextRow();
                }
                ImGui::TableSetColumnIndex(static_cast<int>(placementIndex % 2));
                if (ImGui::Selectable(
                        kPlacementLabels[placementIndex],
                        renderFrame.editorPlacementKindLabel == kPlacementLabels[placementIndex],
                        0,
                        ImVec2(-1.0f, 0.0f))) {
                    queueUiAction(UiActionType::SelectMapEditorPlacementKind, static_cast<std::int32_t>(placementIndex));
                }
            }
            ImGui::EndTable();
        }

        if (renderFrame.editorPlacementKindLabel == "盒体墙" ||
            renderFrame.editorPlacementKindLabel == "道具") {
            if (assetState.objectAssets != nullptr &&
                ImGui::BeginTable("editor-asset-selection", 2,
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 78.0f);
                ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
                drawEditorObjectAssetCombo(renderFrame, *assetState.objectAssets);
                ImGui::EndTable();
            }
        }

        if (assetState.selectedObjectAsset != nullptr) {
            const ImTextureID preview =
                cachedImGuiPreviewTextureForObject(
                    assetState.selectedObjectAsset->thumbnailPath,
                    assetState.selectedObjectAsset->modelPath);
            drawMapEditorAssetPreviewCard(
                renderFrame.editorPlacementKindLabel == "盒体墙" ? "墙体对象预览" : "对象预览",
                preview,
                assetState.selectedObjectAsset->category,
                joinTags(assetState.selectedObjectAsset->tags),
                assetState.selectedObjectAsset->modelPath.generic_string(),
                assetState.selectedObjectAsset->materialPath.generic_string());
            ImGui::TextWrapped(
                "当前对象 id: %s，碰撞盒尺寸 %.2f x %.2f x %.2f。",
                assetState.selectedObjectAsset->id.c_str(),
                assetState.selectedObjectAsset->collisionHalfExtents.x * 2.0f,
                assetState.selectedObjectAsset->collisionHalfExtents.y * 2.0f,
                assetState.selectedObjectAsset->collisionHalfExtents.z * 2.0f);
        }

        ImGui::TextWrapped("光标指中的位置会显示放置虚影。左键或 Enter 立即放置，R 旋转，Tab 切换缩放。");
    }
