#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <render/shader.h>

#include <vector>
#include <iostream>
#include <unordered_map>
#define _USE_MATH_DEFINES
#include <math.h>
#include <iomanip>

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

static float FoV = 45.0f;
static float zNear = 50.0f;
static float zFar = 3000.0f;

// Lighting control
float reflectance = 0.78;
static glm::vec3 lightIntensity(18.4 * reflectance, 15.6 * reflectance, 8.0 * reflectance);

static glm::vec3 lightPosition(lookat.x - 50, 500.0f, lookat.z);

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 0;
static int shadowMapHeight = 0;

static float depthFoV = 0.f;
static float depthNear = 0.f;
static float depthFar = 0.f;

// Helper flag and function to save depth maps for debugging
static bool saveDepth = false;

struct Vec2Hash {
    std::size_t operator()(const glm::vec2& v) const {
        return std::hash<float>()(v.x) ^ (std::hash<float>()(v.y) << 1);
    }
};

struct Chunk {
    glm::vec2 position;
    std::vector<Building> buildings;
    static const int BUILDINGS_PER_SIDE = 8;
    static const constexpr float GAP = 200.0f;

    void initialize(const glm::vec2& pos, const glm::vec3& lightPos, const glm::vec3& lightIntensity) {
        position = pos;

        // Create buildings only if they don't exist yet
        if (buildings.empty()) {
            for (int i = 0; i < BUILDINGS_PER_SIDE; i++) {
                for (int j = 0; j < BUILDINGS_PER_SIDE; j++) {
                    Building b;
                    float x = (i - BUILDINGS_PER_SIDE/2) * GAP;
                    float z = (j - BUILDINGS_PER_SIDE/2) * GAP;

                    float height = 100.0f + ((i * BUILDINGS_PER_SIDE + j) % 3) * 100.0f;

                    glm::vec3 buildingPos(x, 0, z);
                    b.initialize(buildingPos,
                                 glm::vec3(20, height, 20),
                                 lightPos,
                                 lightIntensity);
                    buildings.push_back(b);
                }
            }
        }

        // Update positions based on chunk position
        updatePosition(pos);
    }

    void updatePosition(const glm::vec2& newPos) {
        position = newPos;
        float chunkWidth = BUILDINGS_PER_SIDE * GAP;
        float baseX = position.x * chunkWidth;
        float baseZ = position.y * chunkWidth;

        // Update each building's position
        for (int i = 0; i < BUILDINGS_PER_SIDE; i++) {
            for (int j = 0; j < BUILDINGS_PER_SIDE; j++) {
                int index = i * BUILDINGS_PER_SIDE + j;
                float x = baseX + (i - BUILDINGS_PER_SIDE/2) * GAP;
                float z = baseZ + (j - BUILDINGS_PER_SIDE/2) * GAP;

                buildings[index].updatePosition(glm::vec3(x, 0, z));
            }
        }
    }

    void cleanup() {
        for (Building& b : buildings) {
            b.cleanup();
        }
        buildings.clear();
    }
};

class ChunkManager {
private:
    std::unordered_map<glm::vec2, Chunk, Vec2Hash> activeChunks;
    std::vector<Chunk> recycledChunks;  // Pool of chunks to reuse
    int renderDistance;
    glm::vec3 lightPosition;
    glm::vec3 lightIntensity;
    glm::vec2 lastUpdatePos;
    float chunkWidth;

public:
    ChunkManager(int distance, const glm::vec3& lightPos, const glm::vec3& lightInt)
            : renderDistance(distance),
              lightPosition(lightPos),
              lightIntensity(lightInt),
              lastUpdatePos(glm::vec2(FLT_MAX))
    {
        chunkWidth = Chunk::BUILDINGS_PER_SIDE * Chunk::GAP;
    }

    glm::vec2 worldToChunkCoords(const glm::vec3& worldPos) {
        return glm::vec2(
                floor(worldPos.x / chunkWidth),
                floor(worldPos.z / chunkWidth)
        );
    }

    void update(const glm::vec3& cameraPos) {
        glm::vec2 currentChunk = worldToChunkCoords(cameraPos);
        if (currentChunk == lastUpdatePos) return;

        glm::vec2 chunkDiff = currentChunk - lastUpdatePos;

        // If first update or large movement, do full reset
        if (lastUpdatePos.x > 1000000.0f || abs(chunkDiff.x) > renderDistance || abs(chunkDiff.y) > renderDistance) {
            // Move all chunks to recycled pool
            for (auto& chunk : activeChunks) {
                recycledChunks.push_back(std::move(chunk.second));
            }
            activeChunks.clear();

            // Create initial chunks, reusing from pool when possible
            for (int x = -renderDistance; x <= renderDistance; x++) {
                for (int z = -renderDistance; z <= renderDistance; z++) {
                    glm::vec2 pos = currentChunk + glm::vec2(x, z);

                    Chunk* chunk;
                    if (!recycledChunks.empty()) {
                        activeChunks[pos] = std::move(recycledChunks.back());
                        recycledChunks.pop_back();
                        chunk = &activeChunks[pos];
                    } else {
                        chunk = &activeChunks[pos];
                    }
                    chunk->initialize(pos, lightPosition, lightIntensity);
                }
            }
        } else {
            // Incremental update - only handle the new edge chunks
            int dx = static_cast<int>(chunkDiff.x);
            int dz = static_cast<int>(chunkDiff.y);

            // Move chunks that are now out of range to recycled pool
            if (dx != 0) {
                int removeX = dx > 0 ? currentChunk.x - renderDistance - 1 : currentChunk.x + renderDistance + 1;
                for (int z = currentChunk.y - renderDistance; z <= currentChunk.y + renderDistance; z++) {
                    auto it = activeChunks.find(glm::vec2(removeX, z));
                    if (it != activeChunks.end()) {
                        recycledChunks.push_back(std::move(it->second));
                        activeChunks.erase(it);
                    }
                }
            }
            if (dz != 0) {
                int removeZ = dz > 0 ? currentChunk.y - renderDistance - 1 : currentChunk.y + renderDistance + 1;
                for (int x = currentChunk.x - renderDistance; x <= currentChunk.x + renderDistance; x++) {
                    auto it = activeChunks.find(glm::vec2(x, removeZ));
                    if (it != activeChunks.end()) {
                        recycledChunks.push_back(std::move(it->second));
                        activeChunks.erase(it);
                    }
                }
            }

            // Add new chunks that are now in range, reusing from pool
            if (dx != 0) {
                int newX = dx > 0 ? currentChunk.x + renderDistance : currentChunk.x - renderDistance;
                for (int z = currentChunk.y - renderDistance; z <= currentChunk.y + renderDistance; z++) {
                    glm::vec2 pos(newX, z);
                    if (activeChunks.find(pos) == activeChunks.end()) {
                        if (!recycledChunks.empty()) {
                            activeChunks[pos] = std::move(recycledChunks.back());
                            recycledChunks.pop_back();
                        }
                        activeChunks[pos].initialize(pos, lightPosition, lightIntensity);
                    }
                }
            }
            if (dz != 0) {
                int newZ = dz > 0 ? currentChunk.y + renderDistance : currentChunk.y - renderDistance;
                for (int x = currentChunk.x - renderDistance; x <= currentChunk.x + renderDistance; x++) {
                    glm::vec2 pos(x, newZ);
                    if (activeChunks.find(pos) == activeChunks.end()) {
                        if (!recycledChunks.empty()) {
                            activeChunks[pos] = std::move(recycledChunks.back());
                            recycledChunks.pop_back();
                        }
                        activeChunks[pos].initialize(pos, lightPosition, lightIntensity);
                    }
                }
            }
        }

        lastUpdatePos = currentChunk;
    }

    void render(const glm::mat4& vp) {
        for (auto& pair : activeChunks) {
            for (Building& building : pair.second.buildings) {
                building.render(vp);
            }
        }
    }

    void cleanup() {
        for (auto& pair : activeChunks) {
            pair.second.cleanup();
        }
        for (auto& chunk : recycledChunks) {
            chunk.cleanup();
        }
        activeChunks.clear();
        recycledChunks.clear();
    }
};

void checkOpenGLState(const char* label) {
    GLint program, vao, array_buffer, element_buffer;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_buffer);

    std::cout << "=== OpenGL State at " << label << " ===" << std::endl;
    std::cout << "Current Program: " << program << std::endl;
    std::cout << "VAO Binding: " << vao << std::endl;
    std::cout << "Array Buffer Binding: " << array_buffer << std::endl;
    std::cout << "Element Buffer Binding: " << element_buffer << std::endl;

    // Check vertex attribute arrays
    for(int i = 0; i < 8; i++) {
        GLint enabled;
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        if(enabled)
            std::cout << "Attribute " << i << " is enabled" << std::endl;
    }
    std::cout << "===========================" << std::endl;
}

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

struct AnimatedModel {
    Model* model;
    float offset;  // Offset for animation timing
    glm::vec3 basePosition; // Store original position for animation

    AnimatedModel(Model* m, const glm::vec3& pos, float timeOffset)
            : model(m), basePosition(pos), offset(timeOffset) {}
};

static std::vector<AnimatedModel> animatedModels;
static const int NUM_MODELS = 5;
static const float MODEL_SPACING = 200.0f;

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

    glm::vec3 skyboxScale(1300.0f);
    skybox.initialize(glm::vec3(0,0,0), skyboxScale);

    Floor floor;
    floor.initialize(glm::vec3(0, 0, 100), glm::vec2(5000,5000), lightPosition, lightIntensity);

    static ChunkManager *chunkManager;
    chunkManager = new ChunkManager(1, lightPosition, lightIntensity);

    std::vector<Model*> modelInstances;
    for (int i = 0; i < NUM_MODELS; i++) {
        float xPos = (i - NUM_MODELS/2) * MODEL_SPACING;  // Spread models along x-axis
        glm::vec3 position(xPos, 400, 0);

        Model* newModel = new Model(
                "../assignment/assets/uploads_files_5572778_PLANE (1).obj",
                position,
                glm::vec3(5)
        );
        modelInstances.push_back(newModel);

        float timeOffset = i * 0.5f;  // Offset animation timing for each model
        animatedModels.emplace_back(newModel, position, timeOffset);
    }

	// Camera setup
    glm::mat4 viewMatrix, projectionMatrix;
	projectionMatrix = glm::perspective(glm::radians(FoV), (float)windowWidth / windowHeight, zNear, zFar);

    // Time and frame rate tracking
    static double lastTime = glfwGetTime();
    float time = 0.0f;			// Animation time
    float fTime = 0.0f;			// Time for measuring fps
    unsigned long frames = 0;

    glBindVertexArray(0);
    for(int i = 0; i < 8; i++) {
        glDisableVertexAttribArray(i);
    }
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	do {
        double currentTime = glfwGetTime();
        float deltaTime = float(currentTime - lastTime);
        lastTime = currentTime;

        time += deltaTime;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        viewMatrix = glm::lookAt(eye_center, lookat, up);
        glm::mat4 vp = projectionMatrix * viewMatrix;

        // Render objects here

        checkOpenGLState("Before skybox");
        //glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        skybox.render(vp);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        //glDisable(GL_DEPTH_TEST);
        checkOpenGLState("After skybox");

        checkOpenGLState("Before floor");
        floor.render(vp);
        floor.updatePosition(glm::vec3(eye_center.x, 0, eye_center.z));
        checkOpenGLState("After floor");

        checkOpenGLState("Before buildings");
        chunkManager->update(eye_center);
        chunkManager->render(vp);
        checkOpenGLState("After buildings");

        checkOpenGLState("Before model");
        for (auto& anim : animatedModels) {
            // Calculate new position with z-axis animation
            glm::vec3 newPos = anim.basePosition;
            newPos.z += sin(time * 2.0f + anim.offset) * 100.0f + lookat.z;
            newPos.x += lookat.x;

            // Update model's modelMatrix for the new position
            anim.model->pos = newPos;
            anim.model->Draw(vp);
        }
        checkOpenGLState("After model");

        // FPS tracking
        // Count number of frames over a few seconds and take average
        frames++;
        fTime += deltaTime;
        if (fTime > 2.0f) {
            float fps = frames / fTime;
            frames = 0;
            fTime = 0;

            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << "Frames per second (FPS): " << fps;
            glfwSetWindowTitle(window, stream.str().c_str());
        }

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

    chunkManager->cleanup();

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
