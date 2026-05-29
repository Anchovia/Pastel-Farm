#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    bool shouldClose() const;
    void pollEvents();

    GLFWwindow* handle() const { return m_window; }
    int width()  const { return m_width; }
    int height() const { return m_height; }

    bool wasResized() const { return m_resized; }
    void resetResized()     { m_resized = false; }

private:
    static void resizeCallback(GLFWwindow* window, int w, int h);

    GLFWwindow* m_window = nullptr;
    int  m_width, m_height;
    bool m_resized = false;
};
