#define STB_IMAGE_IMPLEMENTATION
#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 
#include "glm/gtc/matrix_transform.hpp"
#include "stb_image.h"
#include "glm/glm.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shader.h"
#include "mesh.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

//从文件中提取出texture
unsigned int TextureFromFile(const char* path, const string& directory, bool gamma = false);

class Model
{
public:
    vector<Texture> textures_loaded;	// 存储到目前为止加载的所有纹理，优化以确保纹理不会多次加载。
    vector<Mesh>    meshes;       //网格
    string directory;       //Model的名字
    bool gammaCorrection;   //伽马矫正
    // 构造函数：期望将给定的文件路径构造成为3D模型。
    Model(string const& path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }
    // 绘制模型，从而绘制所有网格
    void Draw(Shader shader) {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);     //对model的每个网格执行绘制
    }
private:
    /*  函数   */
    //loadModel函数使用Assimp加载模型致scene中（即Assimp数据接口根对象）
    void loadModel(string path) {

        Assimp::Importer importer;
        //第一个参数为路径，第二个参数为需求:
        //aiProcess_Triangulate:如果模型不是（全部）由三角形组成，它需要将模型所有的图元形状变换为三角形
        //aiProcess_GenNormals：如果模型不包含法向量的话，就为每个顶点创建法线。
        //aiProcess_FlipUVs将在处理的时候翻转y轴的纹理坐标
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode){
            cout << "ERROR::ASSIMP::" << importer.GetErrorString() << endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));//将名字赋给directory

        processNode(scene->mRootNode, scene);//将根节点进行处理，将其中的mesh放入meshes保存
    }
    //获取Assimp网格索引，获取每个网格，处理每个网格，接着对每个节点的子节点重复这一过程。
    void processNode(aiNode* node, const aiScene* scene) {
        // 处理node节点所有的网格（如果有的话）
        for (unsigned int i = 0; i < node->mNumMeshes; i++){
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];//node的mMashes里面储存对应mash在scene的mMeshes里面的编号
            meshes.push_back(processMesh(mesh, scene));
        }
        // 接下来对node的子节点重复这一过程
        for (unsigned int i = 0; i < node->mNumChildren; i++){
            processNode(node->mChildren[i], scene);
        }
    }
    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        vector<Vertex> vertices;//顶点集
        vector<unsigned int> indices;//
        vector<Texture> textures;

        //遍历所有的顶点并将它们放置于vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++){
            Vertex vertex;
            // 处理顶点位置、法线和纹理坐标
            glm::vec3 vector;
            //获取顶点
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            //获取法线
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            //获取纹理坐标
            if (mesh->mTextureCoords[0]){ // 网格是否有纹理坐标？
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                // 切线
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                // 双切线
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            vertices.push_back(vertex);
        }
        // 处理索引：Assimp的接口定义了每个网格都有一个面(Face)数组
        // 每个面代表了一个图元，由于使用了aiProcess_Triangulate，它总是三角形
        for (unsigned int i = 0; i < mesh->mNumFaces; i++){
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)//face的mNumIndices表示这个面由多少顶点构成
                indices.push_back(face.mIndices[j]);
        }
        // 处理材质
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        // 漫反射命名: texture_diffuseN
        // 镜面反射命名: texture_specularN
        // 法线命名: texture_normalN

        // 1. 漫反射贴图，用枚举类将编号1用更通俗的aiTextureType_DIFFUSE表示
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. 镜面反射贴图
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. 法线贴图
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height贴图
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

        return Mesh(vertices, indices, textures);//用顶点集，角标集，纹理集构建Mesh类
    }
    // 检查给定类型的所有材料纹理，如果纹理尚未加载，加载纹理所需的信息作为纹理结构返回。
    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)//检查是否存在该纹理
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true;//存在就跳出循环并且将skip值设置为正确
                    break;
                }
            }
            if (!skip)//skip=0表示没有储存该纹理
            {   // 如果纹理还没有被加载，则加载它
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture); // 添加到已加载的纹理中
            }
        }
        return textures;
    }
};

unsigned int TextureFromFile(const char* path, const string& directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);//纹理ID

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);//加载纹理的参数以及将纹理信息加载到data上，nrComponents表示纹理颜色的表达方式（几个参数表示颜色）
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;//用红色指数表示颜色
        else if (nrComponents == 3)
            format = GL_RGB;//用RGB表示颜色
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

#endif
