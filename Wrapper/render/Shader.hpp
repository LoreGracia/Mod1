#pragma once
#include <glad/gl.h>

#include <string>
#include <type_ptr.hpp>
#include <unordered_map>

class Shader
{
public:
	Shader() = default;
	~Shader();
	Shader(Shader&&) noexcept;
	Shader& operator=(Shader&&) noexcept;
	Shader(const Shader&)			 = delete;
	Shader& operator=(const Shader&) = delete;

	bool loadFromSource(const std::string& vertSrc, const std::string& fragSrc);
	bool loadFromFile(const std::string& vertPath, const std::string& fragPath);

	void   use() const { glUseProgram(m_program); }
	GLuint id() const { return m_program; }

	void setInt(const char* name, int v) const;
	void setFloat(const char* name, float v) const;
	void setVec3(const char* name, const glm::vec3& v) const;
	void setVec4(const char* name, const glm::vec4& v) const;
	void setMat4(const char* name, const glm::mat4& m) const;

private:
	GLint  location(const char* name) const;
	GLuint compile(GLenum type, const std::string& src);

	GLuint										   m_program = 0;
	mutable std::unordered_map<std::string, GLint> m_uniformCache;
};
