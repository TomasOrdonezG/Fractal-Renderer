#ifndef APP_H
#define APP_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <iostream>
#include <stdio.h>
#include <vector>
#include <math.h>
#include <glm/glm.hpp>
#include "debug.h"
#include "utils.h"
#include "shader.h"
#include "window.h"
#include "fullQuad.h"
#include "renderer.h"

class App
{
public:

    App(int windowWidth, int windowHeight)
    {
        // GLFW
        glfwSetErrorCallback(errorCallBack);
        glfwInit();

        // GLFW window hints and context creation
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Full-screen window
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        window = glfwCreateWindow(mode->width, mode->height, "Ray Tracing", primaryMonitor, NULL);
        // window = glfwCreateWindow(600, 600, "Ray Tracing", NULL, NULL); // Original..
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);  // Enable vsync

        // Initialize GLAD
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        
        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Properties of the ImGui window containing the OpenGL texture (that we draw on)
        sceneWindow = Window(windowWidth, windowHeight);

        initImGui();
        quad.init();
        renderer = Renderer(&sceneWindow);
    }

    ~App()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void loop()
    {
        while (!glfwWindowShouldClose(window))
        {
            beginFrame();
            gui();

            ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                pollEvents();

                // Get previous frame texture unit and bind it (this way we can use it in the scene shader)
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sceneWindow.textures[!pingpong]);

                // Bind and clear current frame buffer (this way anything we render gets rendered on this FBO's texture)
                glBindFramebuffer(GL_FRAMEBUFFER, sceneWindow.FBOs[pingpong]);
                glViewport(0, 0, sceneWindow.width, sceneWindow.height);
                glClear(GL_COLOR_BUFFER_BIT);
                
                // Render the scene
                renderer.renderScene(0, &quad);

                // Unbind current FBO and previous texture
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
                
                // Display current texture on ImGui window
                ImGui::ImageButton((void*)sceneWindow.textures[pingpong], ImVec2(sceneWindow.width, sceneWindow.height), ImVec2(0, 1), ImVec2(1, 0), 0);
                
                // Swap pingpong boolean for the next iteration
                pingpong = !pingpong;
            }
            ImGui::End();
            
            endFrame();
        }
    }

private:

    GLFWwindow *window;
    FullQuad quad;
    Window sceneWindow;
    Renderer renderer;
    bool pingpong = false;


    // * GUI

    void gui()
    {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        ImGui::Begin("Data");
        renderer.dataGui();
        ImGui::End();

        ImGui::Begin("Rendering");
        renderer.renderingMenu();
        ImGui::End();

        ImGui::Begin("Fractal");
        renderer.fractalMenu();
        ImGui::End();

        ImGui::Begin("Colour");
        renderer.colourMenu();
        ImGui::End();
    }

    // * General app methods

    void beginFrame()
    {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    
    void endFrame()
    {
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
        glfwSwapBuffers(window);
    }

    void pollEvents()
    {
        static bool isMouseDragging = false;
        static ImVec2 lastMousePos, lastWindowSize;
        
        // Mouse and window attributes
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        ImVec2 windowPos = ImGui::GetCursorScreenPos();
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 mousePosRelative(mousePos.x - windowPos.x, mousePos.y - windowPos.y);
        
        // Resize Window, textures, etc
        bool windowChangedSize = windowSize.x != lastWindowSize.x || windowSize.y != lastWindowSize.y;
        if (windowChangedSize)
        {
            sceneWindow.updateDimensions((int)windowSize.x, (int)windowSize.y);
            renderer.setResolution(sceneWindow.resolution());
        }

        // Check if cursor is inside window
        bool mouseInsideWindow = mousePosRelative.x >= 0 && mousePosRelative.x <= windowSize.x && mousePosRelative.y >= 0 && mousePosRelative.y <= windowSize.y;
        renderer.setZoomOn(mouseInsideWindow, mousePos);
        if (!mouseInsideWindow)
        {
            isMouseDragging = false;
            return;
        }
        
        // Mouse down and release events
        bool leftMouseClick = false;
        ImVec2 lastClickedPosLeft = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
        ImVec2 lastClickedPosLeftRelative = ImVec2(lastClickedPosLeft.x - windowPos.x, lastClickedPosLeft.y - windowPos.y);
        bool lastLeftClickInsideWindow = lastClickedPosLeftRelative.x >= 0 && lastClickedPosLeftRelative.x <= windowSize.x && lastClickedPosLeftRelative.y >= 0 && lastClickedPosLeftRelative.y <= windowSize.y;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && lastLeftClickInsideWindow)
        {
            isMouseDragging = mousePos.x != lastClickedPosLeft.x || mousePos.y != lastClickedPosLeft.y;
        }
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            isMouseDragging = false;
            leftMouseClick = lastClickedPosLeft.x == mousePos.x && lastClickedPosLeft.y == mousePos.y;
        }
        
        // Mouse click callback
        if (leftMouseClick && ImGui::IsWindowFocused())
        {
            renderer.mouseClickCallback(mousePosRelative);
        }

        // Mouse dragging callback
        if (isMouseDragging && ImGui::IsWindowFocused())
        {
            ImVec2 dpos(mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y);
            renderer.mouseDragCallback(dpos);
        }

        // Mouse scrolling callback
        float yOffset = ImGui::GetIO().MouseWheel;
        if (yOffset)
        {
            renderer.mouseScrollCallback(yOffset);
        }

        // Save data for previous frame
        lastMousePos = mousePos;
        lastWindowSize = windowSize;
    }

    void initImGui()
    {
        // Context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; 

        // Style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Renderer backend
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

};

#endif