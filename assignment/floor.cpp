#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION

#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

#include <render/shader.h>

struct Floor {
    glm::vec3 position;        // Position of the floor
    glm::vec2 scale;           // Size of the floor in X and Z directions
    glm::vec3 lightIntensity, lightPosition;

    GLfloat vertex_buffer_data[12] = {    // Vertex definition for a flat quad
            -1.0f, 0.0f, -1.0f,   // Bottom-left
            1.0f, 0.0f, -1.0f,   // Bottom-right
            1.0f, 0.0f,  1.0f,   // Top-right
            -1.0f, 0.0f,  1.0f    // Top-left
    };

    GLuint index_buffer_data[6] = {        // Indices for two triangles
            0, 1, 2,
            0, 2, 3
    };

    GLfloat uv_buffer_data[8] = {          // UV mapping for tiling the texture
            0.0f, 0.0f,  // Bottom-left
            1.0f, 0.0f,  // Bottom-right
            1.0f, 1.0f,  // Top-right
            0.0f, 1.0f   // Top-left
    };

    GLfloat normal_buffer_data[12] = {
            0.0f, 1.0f, 0.0f,  // Bottom-left
            0.0f, 1.0f, 0.0f,  // Bottom-right
            0.0f, 1.0f, 0.0f,  // Top-right
            0.0f, 1.0f, 0.0f   // Top-left
    };

    void updatePosition(glm::vec3 newPosition) {
        position = newPosition;
    }

    GLuint vertexArrayID, vertexBufferID, indexBufferID, uvBufferID, textureID, normalBufferID;
    GLuint mvpMatrixID, textureSamplerID, lightPositionID, lightIntensityID, programID;

    void initialize(glm::vec3 position, glm::vec2 scale, glm::vec3 __lightPosition, glm::vec3 __lightIntensity) {
        for (int i = 0; i < 24; ++i) uv_buffer_data[i] *= 10;

        this->position = position;
        this->scale = scale;
        this->lightIntensity = __lightIntensity;
        this->lightPosition = __lightPosition;

        // Generate and bind vertex array object
        glGenVertexArrays(1, &vertexArrayID);
        glBindVertexArray(vertexArrayID);

        // Generate and bind vertex buffer
        glGenBuffers(1, &vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

        // Generate and bind UV buffer
        glGenBuffers(1, &uvBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

        // Generate and bind index buffer
        glGenBuffers(1, &indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

        glGenBuffers(1, &normalBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

        // Load the texture
        char *textureFilePath = "../assignment/assets/floor.jpg";
        textureID = LoadTextureTileBox(textureFilePath);

        // Load shaders
        programID = LoadShadersFromFile("../assignment/shaders/standardObj.vert",
                                        "../assignment/shaders/standardObj.frag");
        if (programID == 0) {
            std::cerr << "Failed to load shaders." << std::endl;
        }

        // Get uniform locations
        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        textureSamplerID = glGetUniformLocation(programID, "textureSampler");
        lightPositionID = glGetUniformLocation(programID, "lightPosition");
        lightIntensityID = glGetUniformLocation(programID, "lightIntensity");

        GLenum errorCode = glGetError();
        if (errorCode != 0) {
            std::cout << "Floor object error initializing: " << errorCode << std::endl;
        }
    }

    void render(glm::mat4 cameraMatrix) {
        glUseProgram(programID);

        glBindVertexArray(vertexArrayID);

        // Bind vertex buffer
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        // Bind UV buffer
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(textureSamplerID, 0);

        // Set transformation
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), position);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scale.x, 1, scale.y));

        glm::mat4 mvp = cameraMatrix * modelMatrix;

        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

        glUniform3fv(lightPositionID, 1, &lightPosition[0]);
        glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

        // Draw elements
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);

        GLenum errorCode = glGetError();
        if (errorCode != 0) {
            std::cout << "Floor object rendering: " << errorCode << std::endl;
        }
    }

    void cleanup() {
        glDeleteBuffers(1, &vertexBufferID);
        glDeleteBuffers(1, &uvBufferID);
        glDeleteBuffers(1, &indexBufferID);
        glDeleteBuffers(1, &normalBufferID);
        glDeleteTextures(1, &textureID);
        glDeleteVertexArrays(1, &vertexArrayID);
        glDeleteProgram(programID);
    }
};
