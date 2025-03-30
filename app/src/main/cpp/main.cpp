#include <filesystem>
#include <iostream>
#include <libenvpp/env.hpp>
#include <unistd.h>

#include "3dgs.h"
#include "Renderer.h"   // added to access Renderer class to pass touch events
#include "GSScene.h"
#include "args.hxx"
#include "spdlog/spdlog.h"

#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "base_utils.h"

static std::unique_ptr<VulkanSplatting> vulkanSplatting = nullptr;
/*
 * Key events filter to GameActivity's android_native_app_glue. This sample does
 * not use/process any input events, return false for all input events so system
 * can still process them.
 */
extern "C" bool VulkanKeyEventFilter(const GameActivityKeyEvent *event) {
    return false;
}

// marciric : Filter for touch events by user
extern "C" bool VulkanMotionEventFilter(const GameActivityMotionEvent *event) {
    int action = event->action & AMOTION_EVENT_ACTION_MASK;
    float x = GameActivityPointerAxes_getX(&event->pointers[0]);
    float y = GameActivityPointerAxes_getY(&event->pointers[0]);

    if(!vulkanSplatting){
        return false;
    }
    Renderer * renderer = vulkanSplatting->getRenderer();
    if (!renderer)
        return false;

    // Define a touch area for the resolution toggle button
    float screenHeight = renderer->getWindowHeight();
    float screenWidth = renderer->getWindowWidth();

    // same as GuiManager.cpp...
    // TODO: find a way to only define this once
    float buttonWidth = 300;
    float buttonHeight = 70;

    // top left corner of resolution button
    float resolution_button_x = (screenWidth - buttonWidth) / 4;
    float resolution_button_y = screenHeight - 80;

    // Define a touch area for metrics toggle button
    bool isInResolutionButtonArea =
            (x >= resolution_button_x) && (x <= (resolution_button_x + buttonWidth)) &&
            (y >= resolution_button_y) && (y <= (resolution_button_y + buttonHeight));

    // top left corner of GUI button
    float gui_button_x = 3*(screenWidth - buttonWidth) / 4;
    float gui_button_y = screenHeight - 80;

    bool isInGUIButtonArea =
            (x >= gui_button_x) && (x <= (gui_button_x + buttonWidth)) &&
            (y >= gui_button_y) && (y <= (gui_button_y + buttonHeight));

    using namespace std::chrono;
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN: {
            renderer->isTouching = true;
            renderer->initialTouchX = x;
            renderer->initialTouchY = y;
            renderer->lastX = x;
            renderer->lastY = y;
            renderer->lastDownTime = steady_clock::now();   // Record the time when finger touches down.

             // Check for double tap: if the time since last tap is less than 300ms, set the zoomInRequested flag.
            auto now = steady_clock::now();
            auto dt = duration_cast<milliseconds>(now - renderer->lastTapTime).count();
            if (dt < 300) {
                renderer->doubleTap = true;
            } else {
                renderer->doubleTap = false;
            }
            renderer->lastTapTime = now;
            
            // Check if tapped in the resolution toggle area (bottom part of screen)
            if (isInResolutionButtonArea) {
                // Toggle resolution
                bool currentRes = renderer->isUsingHalfResolution();
                renderer->setHalfResolution(!currentRes);
                
                // Log the change
                LOGD("Resolution changed to: %s", !currentRes ? "HALF" : "FULL");
                return true;
            }

            if (isInGUIButtonArea) {
                // Toggle resolution
                bool currentGui = renderer->isUsingGui();
                renderer->setGui(!currentGui);
                return true;
            }
            break;
        }
        case AMOTION_EVENT_ACTION_MOVE:
            if (renderer->isTouching) {
                renderer->touchDeltaX = (x - renderer->lastX);
                renderer->touchDeltaY = (y - renderer->lastY);
                renderer->lastX = x;
                renderer->lastY = y;
            }
            break;
        case AMOTION_EVENT_ACTION_UP:
            renderer->isTouching = false;
            renderer->holdTap = false;
            break;
        case AMOTION_EVENT_ACTION_CANCEL:
            renderer->isTouching = false;
            renderer->holdTap = false;
            break;
        default:
            break;
    }
    return true; // Event was handled
}

void file_to_string(const char * file_name, AAssetManager *assetManager, std::string & output){
    AAsset *asset = AAssetManager_open(assetManager, file_name, AASSET_MODE_BUFFER);
    // check that the scene file exists
    if (asset == nullptr) {
        LOGO("File does not exist: %s", file_name);
        return;
    }

    // read file into a string and pass it to be parsed via VulkanSplatting.configuration
    size_t size = AAsset_getLength(asset);
    const void *buffer = AAsset_getBuffer(asset);
    std::string ret(static_cast<const char*>(buffer), size);
    output = ret;

    AAsset_close(asset);
}

void processForProfiler(
        AAssetManager * assetManager,
        const char * posePath,
        ProfilingMode profilingMode,
        std::vector<glm::mat3x3> & rotations,
        std::vector<glm::vec3> & translations)
{
    if(profilingMode == PSNR) {
        cnpy::NpyArray arr = cnpy::npy_load(assetManager, posePath);
        std::vector<double> vec = arr.as_vec<double>();

//        int elems_to_read = vec.size();
        int elems_to_read = 1;

        for (int view_i = 0; view_i < elems_to_read * arr.shape[1]; view_i += arr.shape[1]) {
            glm::mat3x3 cur_rot;
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 3; k++) {
                    int index = view_i + j * 5;
                    cur_rot[j][k] = float(vec[index + k]);
                }
            }
            rotations.push_back(cur_rot);

            glm::vec3 cur_trans = glm::vec3{
                    static_cast<float>(vec[view_i + 3]),
                    static_cast<float>(vec[view_i + 8]),
                    static_cast<float>(vec[view_i + 13]),
            };
            translations.push_back(cur_trans);
        }
    }
}

int main_cpp(const char *pose_path, android_app *state) {

    ProfilingMode profilingMode;
    bool useValidationLayers;
    bool noGuiFlag;
#ifdef DEBUG
    profilingMode = NONE;
    noGuiFlag = false;

    // for testing and debugging
    useValidationLayers = true;
#else
    // set this manually to what you want to measure
    profilingMode = PSNR;
    noGuiFlag = true;

    // for testing and debugging
    useValidationLayers = false;
#endif


    auto pre = env::prefix("VKGS");
    auto validationLayers = pre.register_variable<bool>("VALIDATION_LAYERS");
    auto physicalDeviceId = pre.register_variable<uint8_t>("PHYSICAL_DEVICE");
    auto immediateSwapchain = pre.register_variable<bool>("IMMEDIATE_SWAPCHAIN");
    auto envVars = pre.parse_and_validate();

    AAssetManager *assetManager = state->activity->assetManager;

    int scene_path_index = 0;
    std::vector<std::string> scene_paths = {"point_cloud.ply", "export.ply", "afshin_27k.ply"};
    int num_scenes = scene_paths.size();

    while(true){
        android_app_set_motion_event_filter(state, NULL);
        vulkanSplatting.reset();

        LOGD("Loading .ply file %s", scene_paths[scene_path_index].c_str());
        std::string assetContent;
        const char * scene_path = scene_paths[scene_path_index].c_str();
        file_to_string(scene_path, assetManager, assetContent);

        std::vector<glm::mat3x3> rotations;
        std::vector<glm::vec3> translations;
        if(profilingMode == PSNR) {
            processForProfiler(assetManager, pose_path, profilingMode, rotations, translations);
        }

        LOGD("Configuring Renderer...");
        VulkanSplatting::RendererConfiguration config{
                useValidationLayers,
                envVars.get(physicalDeviceId).has_value()
                ? std::make_optional(envVars.get(physicalDeviceId).value())
                : std::nullopt,
                envVars.get_or(immediateSwapchain, false),
                assetContent,
                .profilingMode = profilingMode,
                .rotations = rotations,
                .translations = translations,
                .assetManager = assetManager,
        };

        int validationLayersFlag = 0;
        int physicalDeviceIdFlag = 0;
        int immediateSwapchainFlag = 0;

        if (validationLayersFlag) {
            config.enableVulkanValidationLayers = validationLayersFlag;
        }

        if (physicalDeviceIdFlag) {
            config.physicalDeviceId = std::make_optional<uint8_t>(static_cast<uint8_t>(0));
        }

        if (immediateSwapchainFlag) {
            config.immediateSwapchain = immediateSwapchainFlag;
        }

        if (noGuiFlag) {
            config.enableGui = false;
        } else {
            config.enableGui = true;
        }

        int width = 0;
        int height = 0;
        config.window = VulkanSplatting::createAndroidWindow(state->window, width, height);

        // auto renderer = VulkanSplatting(config);
        vulkanSplatting = std::make_unique<VulkanSplatting>(config);
        vulkanSplatting->initialize(scene_path_index);
        scene_path_index = (scene_path_index + 1) % num_scenes;

        // read touch inputs
        state->motionEventFilter = VulkanMotionEventFilter;
        android_app_set_motion_event_filter(state, VulkanMotionEventFilter);

        vulkanSplatting->run();

    }

    return 0;
}

void handleAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGD("APP_CMD_INIT_WINDOW: Window is ready.");
            break;
        case APP_CMD_TERM_WINDOW:
            //LOGD("APP_CMD_TERM_WINDOW: Window is destroyed.");
            break;
        case APP_CMD_GAINED_FOCUS:
            //LOGD("APP_CMD_GAINED_FOCUS: App gained focus.");
            break;
        case APP_CMD_LOST_FOCUS:
            //LOGD("APP_CMD_LOST_FOCUS: App lost focus.");
            break;
        default:
            //LOGD("Unhandled app command: %d", cmd);
            break;
    }
}

void android_main(struct android_app *state) {
    LOGD("in android main");
    // Assign command handler
    state->onAppCmd = handleAppCmd;


    // wait for window
    while (!state->window) {

        int events;
        android_poll_source* source;

        // Poll and process events
        if (ALooper_pollAll(-1, nullptr, &events, (void**)&source) >= 0) {
            if (source) {
                source->process(state, source);
            }
        }
    }

    const char * poses_path = "poses_bounds.npy";

    main_cpp(poses_path, state);
}
