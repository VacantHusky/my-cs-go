    // Aggregates private ImGui implementation slices for the renderer.
    #include "VulkanRendererImGuiCore.inl"
    #include "VulkanRendererImGuiMultiplayer.inl"
    #include "VulkanRendererImGuiMapEditor.inl"

    void buildImGuiWindows(const RenderFrame& renderFrame) {
        if (renderFrame.appFlow != app::AppFlow::MultiPlayerLobby &&
            renderFrame.appFlow != app::AppFlow::MapEditor) {
            multiplayerDraftInitialized_ = false;
            return;
        }

        switch (renderFrame.appFlow) {
            case app::AppFlow::MultiPlayerLobby:
            {
                buildMultiplayerWindow(renderFrame);
                break;
            }
            case app::AppFlow::MapEditor:
            {
                buildMapEditorWindow(renderFrame);
                break;
            }
            case app::AppFlow::MainMenu:
            case app::AppFlow::MapBrowser:
            case app::AppFlow::Settings:
            case app::AppFlow::Exit:
            case app::AppFlow::SinglePlayerLobby:
                break;
        }
    }
