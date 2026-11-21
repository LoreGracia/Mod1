#pragma once
#include <iostream>
#include <memory>

#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <glad/gl.h>

#include <Mesh.hpp>
#include <Shader.hpp>


class Terrain
{
    private:
            std::shared_ptr<Shader> shader;
            std::shared_ptr<Mesh>   plane;
            float size = 2;
            float m_minHeight = 0.f;
            float m_maxHeight = 0.f;
            int m_resolution = 128;
            float originalPeak = 0.f;

            /**
             * @brief R32F texture holding the generated heightfield.
             *
             * Texel (x, z) stores the same elevation as terrain vertex (x, z).
             */
            GLuint m_heightTexture = 0;

            std::vector<glm::vec3> points;
            std::vector<uint32_t>  indices;
	    std::vector<glm::vec3> vertices;
            std::vector<float> normals;

            void getNormals();
            void generateTerrain();
            void parse(const std::string& mapPath);

            const std::vector<float> extractNumbers(const std::string& input);

            /** @brief Releases the OpenGL resources owned by this instance. */
            void release();

    public:
        Terrain(const std::string mapPath);
        Terrain(const char* arg);
        Terrain& operator=(Terrain&& t) noexcept;
        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        ~Terrain();

        /**
         * @brief Creates the GPU resources of the terrain.
         *
         * Must be called once a valid OpenGL context is current. The height
         * texture and the mesh are generated from the parsed heightfield.
         */
        void init();

        /**
         * @brief Returns the shared terrain height texture.
         * @return OpenGL handle of the R32F height texture, or zero before init().
         */
        GLuint heightTexture() const { return m_heightTexture; }

        /**
         * @brief Returns the minimum XZ corner of the terrain footprint.
         * @return World-space (x, z) minimum corner.
         */
        glm::vec2 footprintMin() const { return glm::vec2(-size * 0.5f, -size * 0.5f); }

        /**
         * @brief Returns the maximum XZ corner of the terrain footprint.
         * @return World-space (x, z) maximum corner.
         */
        glm::vec2 footprintMax() const { return glm::vec2(size * 0.5f, size * 0.5f); }

        /** @brief Returns the lowest elevation of the generated heightfield. */
        float minHeight() const { return m_minHeight; }

        /** @brief Returns the highest elevation of the generated heightfield. */
        float maxHeight() const { return m_maxHeight; }

        /**
         * @brief Samples the generated heightfield at a world-space XZ position.
         *
         * Bilinear interpolation matches the R32F texture sampled by the water
         * shaders. Coordinates outside the footprint clamp to the edge texels.
         *
         * @param xz World-space X and Z coordinates.
         * @return Interpolated terrain elevation in world units.
         */
        float heightAt(glm::vec2 xz) const;

        /**
         * @brief Draws the terrain into the supplied view-projection space.
         *
         * The model matrix is the identity so the terrain, the CPU rain drops
         * and the water meshes all share one global coordinate system.
         *
         * @param viewProj Combined view-projection matrix of the active camera.
         */
        void render(const glm::mat4& viewProj);
};
