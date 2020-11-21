/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include "shader.h"

std::string shader::parse_shader(const char* dir, bool write_string) {
  std::ifstream file(dir);
  std::string line, ret = "";

	std::ofstream outto;
	if (write_string) {
		std::string ss(dir);
		ss += "STRING";
		outto = std::ofstream(ss.c_str());
	}
    
  for (; std::getline(file, line); ret += '\n') {
    ret += line;
		if (write_string) {
			// "...\n" -> gotta learn regex
			outto << char(34) << line << char(92) << "n" << char(34) << '\n';
		}
  }

  return ret;
}

int shader::compile_shader(unsigned int type, const char* src) {
  unsigned int id = glCreateShader(type);
  glShaderSource(id, 1, &src, nullptr);
  glCompileShader(id);

  // check if it compiled
  int res;
  glGetShaderiv(id, GL_COMPILE_STATUS, &res);
  if (res) {
    return id;
  }

  // compilation failed. exception handling
  int len;
  glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
  char* message = (char*)alloca(len * sizeof(char));
  glGetShaderInfoLog(id, len, &len, message);
  std::cout << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : (type == GL_FRAGMENT_SHADER ? "fragment" : "compute")) << " shader:" << std::endl << message << std::endl;
    
  return -1;
}

int shader::get_uniform_location(const char* name) {
  if (uniform_map.find(name) != uniform_map.end()) {
    return uniform_map[name];
  }

  unsigned int location = glGetUniformLocation(program_id, name);

  if (location == -1) {
		std::cout << "couldn't find " << name << " uniform" << std::endl;
    return -1;
  }

  uniform_map[name] = location;
    
  return location;
}

shader::shader(std::string compute, bool read_from_file) {
	std::string strc;
	if (read_from_file) {
		strc = parse_shader(compute.c_str(), false);
	} else {
		strc = compute.c_str();
	}
	unsigned int program = glCreateProgram();
	unsigned int cs = compile_shader(GL_COMPUTE_SHADER, strc.c_str());
	glAttachShader(program, cs);
	glLinkProgram(program);
	glValidateProgram(program);
	glDeleteShader(cs);
	program_id = program;
}

shader::shader(std::string vert, std::string frag, bool read_from_file) {
	std::string strv, strf;
	if (read_from_file) {
		if (vert.size()) strv = parse_shader(vert.c_str(), false);
		strf = parse_shader(frag.c_str(), true);
	} else {
		strv = vert.c_str();
		strf = frag.c_str();
	}

	unsigned int program = glCreateProgram();
	unsigned int vs;
	if (vert.size()) vs = compile_shader(GL_VERTEX_SHADER, strv.c_str());
	unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, strf.c_str());

	if (vert.size()) glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glValidateProgram(program);

	if (vert.size()) glDeleteShader(vs);
	glDeleteShader(fs);

	program_id = program;
}

shader::~shader() {
  glUseProgram(0);
  glDeleteProgram(program_id);
}

void shader::bind() {
	glUseProgram(program_id);
}

void shader::unbind() {
	glUseProgram(0);
}

void shader::set1i(const char* name, int v) {
  glUniform1i(get_uniform_location(name), v);
}

void shader::set2i(const char* name, int v1, int v2) {
  glUniform2i(get_uniform_location(name), v1, v2);
}

void shader::set3i(const char* name, int v1, int v2, int v3) {
  glUniform3i(get_uniform_location(name), v1, v2, v3);
}

void shader::set4i(const char* name, int v1, int v2, int v3, int v4) {
  glUniform4i(get_uniform_location(name), v1, v2, v3, v4);
}

void shader::set1f(const char* name, float v) {
  glUniform1f(get_uniform_location(name), v);
}

void shader::set2f(const char* name, float v1, float v2) {
  glUniform2f(get_uniform_location(name), v1, v2);
}

void shader::set3f(const char* name, float v1, float v2, float v3) {
  glUniform3f(get_uniform_location(name), v1, v2, v3);
}

void shader::set4f(const char* name, float v1, float v2, float v3, float v4) {
  glUniform4f(get_uniform_location(name), v1, v2, v3, v4);
}

void shader::set_mat4fv(const char* name, glm::mat4 matrix) {
	glUniformMatrix4fv(get_uniform_location(name), 1, GL_FALSE, &matrix[0][0]);
}
