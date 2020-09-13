/*
 * MIT License
 * Copyright (c) 2020 Pablo Peñarroja
 */

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include "shader.h"

std::string shader::parse_shader(const char* dir) {
  std::ifstream file(dir);
  std::string line, ret = "";
    
  for (; std::getline(file, line); ret += '\n') {
    ret += line;
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
  std::cout << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader:" << std::endl << message << std::endl;
    
  return -1;
}

int shader::get_uniform_location(const char* name) {
  if (uniform_map.find(name) != uniform_map.end()) {
    return uniform_map[name];
  }

  unsigned int location = glGetUniformLocation(shader_id, name);

  if (location == -1) {
    return -1;
  }

  uniform_map[name] = location;
    
  return location;
}

shader::shader(std::string vert, std::string frag) {
	std::string strv;
	if (vert.size()) strv = parse_shader(vert.c_str());
  std::string strf = parse_shader(frag.c_str());

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

  glUseProgram(program);

  shader_id = program;
}

shader::~shader() {
  glUseProgram(0);
  glDeleteProgram(shader_id);
}

void shader::bind() {
	glUseProgram(shader_id);
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
