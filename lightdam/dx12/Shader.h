#pragma once

#include <memory>
#include <d3d12.h>

struct IDxcBlob;

class Shader
{
public:
    Shader();
    ~Shader();

    Shader(Shader&& temp) : m_shaderBlob(temp.m_shaderBlob) { temp.m_shaderBlob = nullptr; }
    void operator = (Shader&& temp) { this->~Shader(); m_shaderBlob = temp.m_shaderBlob; temp.m_shaderBlob = nullptr; }
    Shader(const Shader&) = delete;
    void operator = (const Shader&) = delete;

    static Shader CompileFromFile(const wchar_t* filename);

    IDxcBlob* GetBlob() const { return m_shaderBlob; }
    D3D12_SHADER_BYTECODE GetByteCode() const;

private:
    IDxcBlob* m_shaderBlob = nullptr;
};

