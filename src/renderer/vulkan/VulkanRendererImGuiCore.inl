    std::array<float, 4> textColor(const VkClearAttachment& attachment) const {
        return {
            attachment.clearValue.color.float32[0],
            attachment.clearValue.color.float32[1],
            attachment.clearValue.color.float32[2],
            attachment.clearValue.color.float32[3],
        };
    }

    static void processNativeEvent(const void* event, void* userData) {
        if (event == nullptr || userData == nullptr) {
            return;
        }

        auto* renderer = static_cast<VulkanRenderer*>(userData);
        const auto* sdlEvent = static_cast<const SDL_Event*>(event);
        switch (sdlEvent->type) {
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                renderer->swapchainDirty_ = true;
                renderer->lastSwapchainResizeEventTick_ = SDL_GetTicks();
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                renderer->swapchainDirty_ = true;
                renderer->lastSwapchainResizeEventTick_ = SDL_GetTicks();
                break;
            default:
                break;
        }

        if (!renderer->imguiInitialized_) {
            return;
        }
        ImGui_ImplSDL3_ProcessEvent(sdlEvent);
    }

    static PFN_vkVoidFunction loadImGuiFunction(const char* functionName, void* userData) {
        if (functionName == nullptr || userData == nullptr) {
            return nullptr;
        }
        return static_cast<VulkanDispatch*>(userData)->loadAny(functionName);
    }

    bool initializeImGuiVulkanBackend() {
        if (imguiVulkanBackendInitialized_) {
            return true;
        }
        if (!ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_0, &VulkanRenderer::loadImGuiFunction, &dispatch_)) {
            spdlog::error("[ImGui] Failed to load Vulkan function table.");
            return false;
        }

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_0;
        initInfo.Instance = instance_;
        initInfo.PhysicalDevice = physicalDevice_;
        initInfo.Device = device_;
        initInfo.QueueFamily = queueFamilyIndex_;
        initInfo.Queue = graphicsQueue_;
        initInfo.DescriptorPoolSize = 256;
        initInfo.MinImageCount = static_cast<std::uint32_t>(std::max<std::size_t>(2, frames_.size()));
        initInfo.ImageCount = static_cast<std::uint32_t>(frames_.size());
        initInfo.PipelineInfoMain.RenderPass = renderPass_;
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.UseDynamicRendering = false;
        initInfo.MinAllocationSize = 1024 * 1024;
        initInfo.CheckVkResultFn = [](VkResult result) {
            if (result != VK_SUCCESS) {
                spdlog::warn("[ImGui] Vulkan backend reported result {}", static_cast<int>(result));
            }
        };
        if (!ImGui_ImplVulkan_Init(&initInfo)) {
            spdlog::error("[ImGui] Failed to initialize Vulkan backend.");
            return false;
        }

        imguiVulkanBackendInitialized_ = true;
        return true;
    }

    void shutdownImGuiVulkanBackend() {
        if (!imguiVulkanBackendInitialized_) {
            return;
        }

        clearImGuiPreviewTextures();
        ImGui_ImplVulkan_Shutdown();
        imguiVulkanBackendInitialized_ = false;
    }

    bool initializeImGui(platform::IWindow& window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = "imgui.ini";
        applyImGuiStyle();

        std::filesystem::path fontPath = resolveImGuiFontPath();
        std::filesystem::path iconFontPath = resolveImGuiIconFontPath();
        ImFontConfig fontConfig{};
        fontConfig.OversampleH = 2;
        fontConfig.OversampleV = 2;
        fontConfig.RasterizerMultiply = 1.05f;
        static const ImWchar glyphRanges[] = {
            0x0020, 0x00FF,
            0x2000, 0x206F,
            0x3000, 0x30FF,
            0x4E00, 0x9FFF,
            0,
        };
        if (!fontPath.empty()) {
            if (io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 20.0f, &fontConfig, glyphRanges) != nullptr) {
                spdlog::info("[ImGui] Loaded UI font: {}", fontPath.generic_string());
            } else {
                spdlog::warn("[ImGui] Failed to load UI font: {}", fontPath.generic_string());
            }
        }
        if (io.Fonts->Fonts.empty()) {
            io.Fonts->AddFontDefault();
            spdlog::warn("[ImGui] Falling back to default font; Chinese glyph coverage may be limited.");
        }
        static const ImWchar iconGlyphRanges[] = {
            0x002B, 0x002B,
            0xF030, 0xF030,
            0xF05B, 0xF05B,
            0xF065, 0xF065,
            0xF1B2, 0xF1B2,
            0xF1F8, 0xF1F8,
            0xF255, 0xF255,
            0xF5EE, 0xF5EE,
            0,
        };
        if (!iconFontPath.empty()) {
            ImFontConfig iconFontConfig{};
            iconFontConfig.MergeMode = true;
            iconFontConfig.PixelSnapH = true;
            iconFontConfig.OversampleH = 2;
            iconFontConfig.OversampleV = 2;
            iconFontConfig.GlyphOffset = ImVec2(0.0f, 1.0f);
            if (io.Fonts->AddFontFromFileTTF(iconFontPath.string().c_str(), 18.0f, &iconFontConfig, iconGlyphRanges) != nullptr) {
                spdlog::info("[ImGui] Loaded icon font: {}", iconFontPath.generic_string());
            } else {
                spdlog::warn("[ImGui] Failed to load icon font: {}", iconFontPath.generic_string());
            }
        }

        if (!ImGui_ImplSDL3_InitForVulkan(sdlWindow_)) {
            spdlog::error("[ImGui] Failed to initialize SDL3 backend.");
            ImGui::DestroyContext();
            return false;
        }
        window.setNativeEventObserver({&VulkanRenderer::processNativeEvent, this});

        if (!initializeImGuiVulkanBackend()) {
            window.setNativeEventObserver({});
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        imguiInitialized_ = true;
        imguiKeyboardCapture_ = false;
        imguiMouseCapture_ = false;
        return true;
    }

    void shutdownImGui() {
        if (!imguiInitialized_) {
            return;
        }

        imguiKeyboardCapture_ = false;
        imguiMouseCapture_ = false;
        shutdownImGuiVulkanBackend();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imguiInitialized_ = false;
    }

    void beginImGuiFrame(const RenderFrame& renderFrame) {
        if (!imguiInitialized_ || !imguiVulkanBackendInitialized_) {
            imguiKeyboardCapture_ = false;
            imguiMouseCapture_ = false;
            return;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        buildImGuiWindows(renderFrame);
        ImGuiIO& io = ImGui::GetIO();
        imguiKeyboardCapture_ = io.WantCaptureKeyboard;
        imguiMouseCapture_ = io.WantCaptureMouse;
        ImGui::Render();
    }

    void queueUiAction(const UiActionType type, const std::int32_t value0 = 0, const std::int32_t value1 = 0) {
        pendingUiActions_.push_back(UiAction{
            .type = type,
            .value0 = value0,
            .value1 = value1,
            .text = {},
            .vectorValue = {},
        });
    }

    void queueUiTextAction(const UiActionType type, std::string text) {
        pendingUiActions_.push_back(UiAction{
            .type = type,
            .text = std::move(text),
            .vectorValue = {},
        });
    }

    void queueUiVec3Action(const UiActionType type, const util::Vec3& vectorValue) {
        pendingUiActions_.push_back(UiAction{
            .type = type,
            .text = {},
            .vectorValue = vectorValue,
        });
    }

    template <std::size_t N>
    void copyStringToBuffer(std::array<char, N>& buffer, const std::string& value) {
        buffer.fill('\0');
        const std::size_t count = std::min(buffer.size() - 1, value.size());
        std::memcpy(buffer.data(), value.data(), count);
    }

    void applyImGuiStyle() {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.WindowPadding = ImVec2(10.0f, 9.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 14.0f;
        style.ScrollbarSize = 12.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.13f, 0.92f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.20f, 0.27f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.19f, 0.31f, 0.42f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.36f, 0.48f, 0.78f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.48f, 0.64f, 0.86f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.55f, 0.72f, 0.94f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.21f, 0.42f, 0.56f, 0.76f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.54f, 0.71f, 0.92f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.60f, 0.78f, 1.00f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.16f, 0.20f, 0.90f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.22f, 0.28f, 0.95f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.28f, 0.34f, 1.00f);
    }

    std::filesystem::path resolveImGuiFontPath() const {
        std::error_code error;
        const std::array<std::filesystem::path, 7> candidates{{
            std::filesystem::path("assets/fonts/NotoSansSC-Variable.ttf"),
#ifdef MYCSGO_ASSET_ROOT
            std::filesystem::path(MYCSGO_ASSET_ROOT) / "fonts" / "NotoSansSC-Variable.ttf",
#else
            std::filesystem::path(),
#endif
#ifdef _WIN32
            std::filesystem::path("C:/Windows/Fonts/msyh.ttc"),
            std::filesystem::path("C:/Windows/Fonts/msyh.ttf"),
            std::filesystem::path("C:/Windows/Fonts/msyhbd.ttc"),
            std::filesystem::path("C:/Windows/Fonts/simhei.ttf"),
            std::filesystem::path("C:/Windows/Fonts/simsun.ttc"),
#else
            std::filesystem::path(),
            std::filesystem::path(),
            std::filesystem::path(),
            std::filesystem::path(),
            std::filesystem::path(),
#endif
        }};

        for (const auto& candidate : candidates) {
            if (candidate.empty()) {
                continue;
            }
            if (std::filesystem::exists(candidate, error) && !error) {
                return candidate;
            }
            error.clear();
        }
        return {};
    }

    std::filesystem::path resolveImGuiIconFontPath() const {
        std::error_code error;
        const std::array<std::filesystem::path, 2> candidates{{
            std::filesystem::path("assets/fonts/fa-solid-900.ttf"),
#ifdef MYCSGO_ASSET_ROOT
            std::filesystem::path(MYCSGO_ASSET_ROOT) / "fonts" / "fa-solid-900.ttf",
#else
            std::filesystem::path(),
#endif
        }};

        for (const auto& candidate : candidates) {
            if (candidate.empty()) {
                continue;
            }
            if (std::filesystem::exists(candidate, error) && !error) {
                return candidate;
            }
            error.clear();
        }
        return {};
    }
