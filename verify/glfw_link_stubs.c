/* Satisfies GLFW symbols so main.cpp can link. The wave check never calls them.
   The repo does not ship the GLFW import library (external/glfw lib-* is gitignored). */
void glfwGetCursorPos(void) {}
void glfwGetFramebufferSize(void) {}
void glfwGetWindowSize(void) {}
int glfwInit(void) { return 0; }
void glfwWindowHint(void) {}
void glfwCreateWindow(void) {}
void glfwMakeContextCurrent(void) {}
void glfwSetFramebufferSizeCallback(void) {}
void glfwSetMouseButtonCallback(void) {}
void glfwGetProcAddress(void) {}
void glfwTerminate(void) {}
int glfwGetKey(void) { return 0; }
void glfwSetWindowShouldClose(void) {}
void glfwSwapBuffers(void) {}
void glfwPollEvents(void) {}
int glfwWindowShouldClose(void) { return 1; }
