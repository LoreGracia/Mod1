#include <glad/gl.h>

#include <Shader.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utils.h>


// namespace
// {
// std::string readFile(const std::string& path)
// {
// 	std::ifstream file(path);
// 	if (!file)
// 		throw std::runtime_error("cannot open " + path);
// 	std::stringstream ss;
// 	ss << file.rdbuf();
// 	return ss.str();
// }
// }  // namespace

Shader::~Shader()
{
	if (m_program)
		glDeleteProgram(m_program);
}

Shader::Shader(Shader&& o) noexcept
	: m_program(o.m_program), m_uniformCache(std::move(o.m_uniformCache))
{
	o.m_program = 0;
}

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

bool Shader::loadFromFile(const std::string& v, const std::string& f)
{
	try {
		return loadFromSource(readFile(v), readFile(f));
	} catch (const std::exception& e) {
		std::cerr << e.what() << "\n";
		return false;
	}
}

GLint Shader::location(const char* name) const
{
	auto it = m_uniformCache.find(name);

	if (it != m_uniformCache.end())
		return it->second;
	GLint loc = glGetUniformLocation(m_program, name);

	m_uniformCache.emplace(name, loc);
	return loc;
}

bool Shader::loadComputeFromSource(const std::string& computeSrc)
{
    GLuint cs = compile(GL_COMPUTE_SHADER, computeSrc);
    if (!cs) return false;

    if (m_program) glDeleteProgram(m_program);
    m_program = glCreateProgram();
    glAttachShader(m_program, cs);
    glLinkProgram(m_program);
    glDeleteShader(cs);   // el shader ya está linkado, liberar

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

bool Shader::loadComputeFromFile(const std::string& computePath)
{
    try { return loadComputeFromSource(readFile(computePath)); }
    catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return false;
    }
}

void Shader::dispatch(GLuint groupsX, GLuint groupsY, GLuint groupsZ) const
{
    glUseProgram(m_program);
    glDispatchCompute(groupsX, groupsY, groupsZ);
    // NO hacemos glMemoryBarrier aquí: el caller sabe qué va a leer
    // y con qué bits. Hacerlo aquí con un barrier "universal" sería caro.
}

void Shader::dispatchForGrid(GLuint gridX, GLuint gridY, GLuint gridZ,
                             GLuint localX, GLuint localY, GLuint localZ) const
{
    // redondeo hacia arriba: ceil(grid / local)
    GLuint gx = (gridX + localX - 1) / localX;
    GLuint gy = (gridY + localY - 1) / localY;
    GLuint gz = (gridZ + localZ - 1) / localZ;
    dispatch(gx, gy, gz);
}



void Shader::setInt(const char* n, int v) const
{
	glUniform1i(location(n), v);
}
void Shader::setFloat(const char* n, float v) const
{
	glUniform1f(location(n), v);
}
void Shader::setVec3(const char* n, const glm::vec3& v) const
{
	glUniform3fv(location(n), 1, glm::value_ptr(v));
}
void Shader::setVec4(const char* n, const glm::vec4& v) const
{
	glUniform4fv(location(n), 1, glm::value_ptr(v));
}
void Shader::setMat4(const char* n, const glm::mat4& m) const
{
	glUniformMatrix4fv(location(n), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::setIVec3(const char* name, const glm::ivec3& v) const {
    glUniform3i(location(name), v.x, v.y, v.z);
}
