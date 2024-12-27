#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION

#include <vector>
#include <iostream>
#include <math.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "render/shader.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        setupMesh();
    }

    void draw(const glm::mat4& projectionView, const glm::mat4& model) {
        glUseProgram(shaderProgram);

        // Send matrices to shader
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projectionView"), 1, GL_FALSE, &projectionView[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glUseProgram(0);
    }

private:
    unsigned int vao, vbo, ebo, shaderProgram;

    void setupMesh() {
        shaderProgram = LoadShadersFromFile("../assignment/shaders/mesh.vert", "../assignment/shaders/mesh.frag");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // vertex positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // vertex normals
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        // vertex texture coords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

        glBindVertexArray(0);
    }
};

class Model {
public:
    glm::vec3 position;
    glm::vec3 scale;
    glm::mat4 modelMatrix;
    std::vector<Mesh> meshes;

    Model(const char* path) {
        loadModel(path);
        position = glm::vec3(0.0f);
        scale = glm::vec3(1.0f);
        modelMatrix = glm::mat4(1.0f);
    }

    void initialize(const glm::vec3& pos, const glm::vec3& scl) {
        position = pos;
        scale = scl;
        updateModelMatrix();
    }

    void draw(const glm::mat4& projectionView) {
        for(unsigned int i = 0; i < meshes.size(); i++) {
            meshes[i].draw(projectionView, modelMatrix);
        }
    }

private:
    std::string directory;
    std::vector<Texture> textures_loaded;

    void updateModelMatrix() {
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, position);
        modelMatrix = glm::scale(modelMatrix, scale);
        // Add rotation if needed for your model orientation
        //modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    void loadModel(std::string path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
                                                 aiProcess_Triangulate |
                                                 aiProcess_GenNormals |
                                                 aiProcess_JoinIdenticalVertices
        );

        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.position = glm::vec3(
                    mesh->mVertices[i].x,
                    mesh->mVertices[i].y,
                    mesh->mVertices[i].z
            );

            if(mesh->HasNormals()) {
                vertex.normal = glm::vec3(
                        mesh->mNormals[i].x,
                        mesh->mNormals[i].y,
                        mesh->mNormals[i].z
                );
            }

            if(mesh->mTextureCoords[0]) {
                vertex.texCoords = glm::vec2(
                        mesh->mTextureCoords[0][i].x,
                        mesh->mTextureCoords[0][i].y
                );
            } else {
                vertex.texCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        return Mesh(vertices, indices, textures);
    }
};