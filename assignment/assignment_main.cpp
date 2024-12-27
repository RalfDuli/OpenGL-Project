#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <render/shader.h>

#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

#include "building.cpp"
#include "floor.cpp"
#include "model.cpp"
#include "skybox.cpp"

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void cursor_callback(GLFWwindow *window, double xpos, double ypos);

// OpenGL camera view parameters
static glm::vec3 eye_center(0, 250, 800);
static glm::vec3 lookat(0, 200, 0); // Remove 200 from lookat, eye_center
static glm::vec3 up(0.0f, 1.0f, 0.0f);

Skybox skybox;

//static float viewAzimuth = 0.f;
//static float viewPolar = 0.f;
//static float viewDistance = 300.0f;

//static glm::vec3 eye_center = glm::vec3(
//        viewDistance * cos(viewAzimuth),
//        viewDistance * cos(viewPolar),
//        viewDistance * sin(viewAzimuth)
//        );

static float FoV = 45.0f;
static float zNear = 50.0f;
static float zFar = 2000.0f; //1500.0f;

// Lighting control
float reflectance = 0.78;
static glm::vec3 lightIntensity(18.4 * reflectance, 15.6 * reflectance, 8.0 * reflectance);

static glm::vec3 lightPosition(lookat.x - 50, 500.0f, lookat.z);

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 0;
static int shadowMapHeight = 0;

// TODO: set these parameters
static float depthFoV = 0.f;
static float depthNear = 0.f;
static float depthFar = 0.f;

// Helper flag and function to save depth maps for debugging
static bool saveDepth = false;

// This function retrieves and stores the depth map of the default frame buffer
// or a particular frame buffer (indicated by FBO ID) to a PNG image.
static void saveDepthTexture(GLuint fbo, std::string filename) {
    int width = shadowMapWidth;
    int height = shadowMapHeight;
	if (shadowMapWidth == 0 || shadowMapHeight == 0) {
		width = windowWidth;
		height = windowHeight;
	}
    int channels = 3;

    std::vector<float> depth(width * height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadBuffer(GL_DEPTH_COMPONENT);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<unsigned char> img(width * height * 3);
    for (int i = 0; i < width * height; ++i) img[3*i] = img[3*i+1] = img[3*i+2] = depth[i] * 255;

    stbi_write_png(filename.c_str(), width, height, channels, img.data(), width * channels);
}

bool isBuildingInView(const glm::vec3 &position, const glm::mat4 &vp) {
    glm::vec4 clipSpacePos = vp* glm::vec4(position, 1.0f);

    if (clipSpacePos.w != 0.0f) {
        clipSpacePos /= clipSpacePos.w;
    }

    return (clipSpacePos.x >= -1.0f && clipSpacePos.x <= 1.0f &&
            clipSpacePos.y >= -1.0f && clipSpacePos.y <= 1.0f &&
            clipSpacePos.z >= -1.0f && clipSpacePos.z <= 1.0f);
}

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Lab 3", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	glfwSetCursorPosCallback(window, cursor_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Prepare shadow map size for shadow mapping. Usually this is the size of the window itself, but on some platforms like Mac this can be 2x the size of the window. Use glfwGetFramebufferSize to get the shadow map size properly.
    glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

    glm::vec3 skyboxScale(900.0f);
    skybox.initialize(glm::vec3(0,0,-0), skyboxScale);

    Floor floor;
    floor.initialize(glm::vec3(0, 0, 100), glm::vec2(5000,5000), lightPosition, lightIntensity);

    std::vector<Building> buildings;
    int numBuildings = 8;
    for (int i = 0; i < numBuildings; i++) {
        Building b;
        float gapBetweenBuildings = 200;

        b.initialize(glm::vec3(-(gapBetweenBuildings * numBuildings / 2) + gapBetweenBuildings * i, 0, 150 ),
                     glm::vec3(20, 160, 20),
                     lightPosition,
                     lightIntensity);
        buildings.push_back(b);
    }

    Model model("../assignment/assets/uploads_files_5572778_PLANE.obj");
    glm::vec3 modelPos(0, 40, 100);
    glm::vec3 modelScl(1);
    model.initialize(modelPos, modelScl);

	// Camera setup
    glm::mat4 viewMatrix, projectionMatrix;
	projectionMatrix = glm::perspective(glm::radians(FoV), (float)windowWidth / windowHeight, zNear, zFar);

	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		viewMatrix = glm::lookAt(eye_center, lookat, up);
		glm::mat4 vp = projectionMatrix * viewMatrix;

		// Render objects here

        skybox.render(vp);

        floor.render(vp);
        floor.updatePosition(glm::vec3(eye_center.x, 0, eye_center.z));

        for (int i = 0; i < numBuildings; i++) {
            Building b = buildings[i];
            b.render(vp);
        }

        //model.draw(vp);

		if (saveDepth) {
            std::string filename = "depth_camera.png";
            saveDepthTexture(0, filename);
            std::cout << "Depth texture saved to " << filename << std::endl;
            saveDepth = false;
        }

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up here

    skybox.cleanup();

    floor.cleanup();

    for (Building &b : buildings) {
        b.cleanup();
    }

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    float movementSpeed = 20.0f;

	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		eye_center = glm::vec3(-278.0f, 273.0f, 800.0f);
		lightPosition = glm::vec3(eye_center.x - 50, 500.0f, eye_center.z);

	}

	if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.z -= movementSpeed;
        lookat.z -= movementSpeed;
        skybox.pos.z -= movementSpeed;
	}

	if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.z += movementSpeed;
        lookat.z += movementSpeed;
        skybox.pos.z += movementSpeed;
	}

	if (key == GLFW_KEY_A && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.x -= movementSpeed;
        lookat.x -= movementSpeed;
        skybox.pos.x -= movementSpeed;
	}

	if (key == GLFW_KEY_D && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.x += movementSpeed;
        lookat.x += movementSpeed;
        skybox.pos.x += movementSpeed;
	}

//	if (key == GLFW_KEY_UP && (action == GLFW_REPEAT || action == GLFW_PRESS))
//	{
//		lightPosition.z -= 20.0f;
//	}
//
//	if (key == GLFW_KEY_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS))
//	{
//		lightPosition.z += 20.0f;
//	}

	if (key == GLFW_KEY_SPACE && (action == GLFW_REPEAT || action == GLFW_PRESS))
    {
        saveDepth = true;
    }

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

static void cursor_callback(GLFWwindow *window, double xpos, double ypos) {
	if (xpos < 0 || xpos >= windowWidth || ypos < 0 || ypos > windowHeight)
		return;

	// Normalize to [0, 1]
	float x = xpos / windowWidth;
	float y = ypos / windowHeight;

	// To [-1, 1] and flip y up
	x = x * 2.0f - 1.0f;
	y = 1.0f - y * 2.0f;

	const float scale = 250.0f;
	lightPosition.x = x * scale - 278;
	lightPosition.y = y * scale + 278;

	//std::cout << lightPosition.x << " " << lightPosition.y << " " << lightPosition.z << std::endl;
}
