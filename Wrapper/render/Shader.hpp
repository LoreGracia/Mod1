#pragma once
#include <glad/gl.h>
#include <string>
#include <unordered_map>

class Shader {
	public:
		Shader() = default;
		~Shader();
		Shader(Shader &&) noexcept;
		Shader &operator=(Shader&&) noexcept;
		Shader(const Shader &)				= delete;
		Shader &operator=(const Shader&)	= delete;

		bool loadFromSource(const std::string &vertSrc, const std::string &fragSrc);
		bool loadFromFile  (const std::string &vertPath, const std::string &fragPath);

		void use()  const { glUseProgram(m_program); }
		GLuint id() const { return m_program; }

		// ---- setters: solo floats, sin glm ----
		void setInt  (const char *name, int v)    const;
		void setFloat(const char *name, float v)  const;

		// vec3: (x, y, z) o puntero a 3 floats
		void setVec3(const char *name, float x, float y, float z) const;
		void setVec3(const char *name, const float* v)            const;

		// vec4
		void setVec4(const char *name, float x, float y, float z, float w)	const;
		void setVec4(const char *name, const float *v) 						const;

		// mat4: puntero a 16 floats, column-major (como glm::value_ptr)
		void setMat4(const char *name, const float *m16)	const;

	private:
		GLint  location(const char *name) const;
		GLuint compile(GLenum type, const std::string &src);

		GLuint m_program = 0;
		mutable std::unordered_map<std::string, GLint> m_uniformCache;
};
