/**
 * @file Shader.cpp
 * @brief Implementation of the Shader class, handling shader compilation,
 * program linking, uniform caching, and compute dispatch.
 */

#include <glad/gl.h>

#include <Shader.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>


namespace
{
/**
 * @brief Helper function to read the entire contents of a file into a string.
 * @param path The path to the file.
 * @return A string containing the file contents.
 * @throw std::runtime_error If the file cannot be opened.
 */
std::string readFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
		throw std::runtime_error("cannot open " + path);
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}
}  // namespace

/**
 * @brief Destructor. Releases the OpenGL program if it exists.
 */
Shader::~Shader()
{
	if (m_program)
		glDeleteProgram(m_program);
}

/**
 * @brief Move constructor. Transfers ownership of the OpenGL program and the uniform cache.
 * @param o The Shader to move from.
 */
Shader::Shader(Shader&& o) noexcept
	: m_program(o.m_program), m_uniformCache(std::move(o.m_uniformCache))
{
	o.m_program = 0;
}

/**
 * @brief Move assignment operator. Releases current program and transfers ownership.
 * @param o The Shader to move from.
 * @return A reference to this Shader.
 */
Shader& Shader::operator=(Shader&& o) noexcept
{
	if (this != &o) {
		if (m_program)
			glDeleteProgram(m_program);
		m_program	   = o.m_program;
		m_uniformCache = std::move(o.m_uniformCache);
		o.m_program	   = 0;
	}
	return *this;
}

/**
 * @brief Compiles a single shader stage from source code.
 * @param type The shader type (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER).
 * @param src The source code string.
 * @return The compiled shader handle, or 0 if compilation failed.
 */
GLuint Shader::compile(GLenum type, const std::string& src)
{
	GLuint		sh = glCreateShader(type);
	const char* c  = src.c_str();

	glShaderSource(sh, 1, &c, nullptr);
	glCompileShader(sh);

	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len);
		glGetShaderInfoLog(sh, len, nullptr, log.data());
		std::cerr << "Shader compile error:\n" << log.data() << "\n";
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

/**
 * @brief Compiles and links a vertex and fragment shader from source strings.
 * @param v Vertex shader source.
 * @param f Fragment shader source.
 * @return True if the program linked successfully, false otherwise.
 */
bool Shader::loadFromSource(const std::string& v, const std::string& f)
{
	GLuint vs = compile(GL_VERTEX_SHADER, v);
	if (!vs)
		return false;
	GLuint fs = compile(GL_FRAGMENT_SHADER, f);
	if (!fs) {
		glDeleteShader(vs);
		return false;
	}

	if (m_program)
		glDeleteProgram(m_program);
	m_program = glCreateProgram();
	glAttachShader(m_program, vs);
	glAttachShader(m_program, fs);
	glLinkProgram(m_program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = 0;

	glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint len = 0;

		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len);
		glGetProgramInfoLog(m_program, len, nullptr, log.data());
		std::cerr << "Program link error:\n" << log.data() << "\n";
		glDeleteProgram(m_program);
		m_program = 0;

		return false;
	}
	m_uniformCache.clear();
	return true;
}

/**
 * @brief Compiles and links a vertex and fragment shader from files.
 * @param v Path to the vertex shader file.
 * @param f Path to the fragment shader file.
 * @return True if loading and linking succeeded, false otherwise.
 */
bool Shader::loadFromFile(const std::string& v, const std::string& f)
{
	try {
		return loadFromSource(readFile(v), readFile(f));
	} catch (const std::exception& e) {
		std::cerr << e.what() << "\n";
		return false;
	}
}

/**
 * @brief Gets the location of a uniform variable, utilizing a cache.
 * @param name The name of the uniform.
 * @return The location of the uniform, or -1 if not found.
 */
GLint Shader::location(const char* name) const
{
	auto it = m_uniformCache.find(name);

	if (it != m_uniformCache.end())
		return it->second;
	GLint loc = glGetUniformLocation(m_program, name);

	m_uniformCache.emplace(name, loc);
	return loc;
}

/**
 * @brief Compiles and links a compute shader from a source string.
 * @param computeSrc The compute shader source.
 * @return True if successful, false otherwise.
 */
bool Shader::loadComputeFromSource(const std::string& computeSrc)
{
	GLuint cs = compile(GL_COMPUTE_SHADER, computeSrc);
	if (!cs)
		return false;

	if (m_program)
		glDeleteProgram(m_program);
	m_program = glCreateProgram();
	glAttachShader(m_program, cs);
	glLinkProgram(m_program);
	glDeleteShader(cs);	 // The shader is linked, safe to delete.

	GLint ok = 0;
	glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len);
		glGetProgramInfoLog(m_program, len, nullptr, log.data());
		std::cerr << "Compute program link error:\n" << log.data() << "\n";
		glDeleteProgram(m_program);
		m_program = 0;
		return false;
	}
	m_uniformCache.clear();
	return true;
}

/**
 * @brief Compiles and links a compute shader from a file.
 * @param computePath Path to the compute shader file.
 * @return True if successful, false otherwise.
 */
bool Shader::loadComputeFromFile(const std::string& computePath)
{
	try {
		return loadComputeFromSource(readFile(computePath));
	} catch (const std::exception& e) {
		std::cerr << e.what() << "\n";
		return false;
	}
}

/**
 * @brief Dispatches a compute shader.
 * @note Does NOT insert a glMemoryBarrier. The caller is responsible for
 * inserting the appropriate memory barriers based on what resources will be read.
 * A universal barrier here would be overly expensive.
 * @param groupsX Number of work groups on the X axis.
 * @param groupsY Number of work groups on the Y axis.
 * @param groupsZ Number of work groups on the Z axis.
 */
void Shader::dispatch(GLuint groupsX, GLuint groupsY, GLuint groupsZ) const
{
	glUseProgram(m_program);
	glDispatchCompute(groupsX, groupsY, groupsZ);
}

/**
 * @brief Calculates the required number of work groups for a grid and dispatches.
 * Uses ceiling division: ceil(grid / local).
 * @param gridX Total grid size X.
 * @param gridY Total grid size Y.
 * @param gridZ Total grid size Z.
 * @param localX Local workgroup size X.
 * @param localY Local workgroup size Y.
 * @param localZ Local workgroup size Z.
 */
void Shader::dispatchForGrid(GLuint gridX, GLuint gridY, GLuint gridZ, GLuint localX, GLuint localY,
							 GLuint localZ) const
{
	GLuint gx = (gridX + localX - 1) / localX;
	GLuint gy = (gridY + localY - 1) / localY;
	GLuint gz = (gridZ + localZ - 1) / localZ;
	dispatch(gx, gy, gz);
}

/**
 * @brief Sets an integer uniform variable.
 * @param n Name of the uniform.
 * @param v Value.
 */
void Shader::setInt(const char* n, int v) const
{
	glUniform1i(location(n), v);
}

/**
 * @brief Sets a float uniform variable.
 * @param n Name of the uniform.
 * @param v Value.
 */
void Shader::setFloat(const char* n, float v) const
{
	glUniform1f(location(n), v);
}

/**
 * @brief Sets a vec3 uniform variable.
 * @param n Name of the uniform.
 * @param v Value.
 */
void Shader::setVec3(const char* n, const glm::vec3& v) const
{
	glUniform3fv(location(n), 1, glm::value_ptr(v));
}

/**
 * @brief Sets a vec4 uniform variable.
 * @param n Name of the uniform.
 * @param v Value.
 */
void Shader::setVec4(const char* n, const glm::vec4& v) const
{
	glUniform4fv(location(n), 1, glm::value_ptr(v));
}

/**
 * @brief Sets a mat4 uniform variable.
 * @param n Name of the uniform.
 * @param m Value.
 */
void Shader::setMat4(const char* n, const glm::mat4& m) const
{
	glUniformMatrix4fv(location(n), 1, GL_FALSE, glm::value_ptr(m));
}

/**
 * @brief Sets an ivec3 uniform variable.
 * @param name Name of the uniform.
 * @param v Value.
 */
void Shader::setIVec3(const char* name, const glm::ivec3& v) const
{
	glUniform3i(location(name), v.x, v.y, v.z);
}
