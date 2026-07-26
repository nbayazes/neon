#include "pch.h"
#include "ShaderCompiler.h"
#include <dxcapi.h>
#include "Graphics/DeviceResources.h"
#include "Logging.h"

// Most of this code is based on https://simoncoenen.com/blog/programming/graphics/DxcCompiling

namespace neon::gfx {

namespace {
    ID3D12Device* _device = nullptr;
    ComPtr<IDxcUtils> _utils;
    ComPtr<IDxcCompiler3> _compiler;
    ComPtr<IDxcIncludeHandler> _includeHandler;

    Dictionary<uintptr_t, CompiledShader> _shaders;
    Dictionary<uintptr_t, CompiledPipeline> _pipelines;

    constexpr auto VERTEX_SHADER_VERSION = L"vs_6_0";
    constexpr auto PIXEL_SHADER_VERSION = L"ps_6_0";
    constexpr auto COMPUTE_SHADER_VERSION = L"cs_6_0";
}

void LogComException(const com_exception& e, ID3DBlob* error) {
    SPDLOG_ERROR(e.what());
    if (error) {
        auto size = error->GetBufferSize();
        auto* msgs = error->GetBufferPointer();
        std::string msg((const char*)msgs, size);
        SPDLOG_ERROR(msg);
    }
}

// Load Root Signature from the shader (must be defined in hlsl)
void LoadShaderRootSig(ID3DBlob& shader, ComPtr<ID3D12RootSignature>& rootSignature) {
    ThrowIfFailed(_device->CreateRootSignature(
        0,
        shader.GetBufferPointer(),
        shader.GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    ));
}

ComPtr<ID3D12RootSignature> LoadShaderRootSig(ID3DBlob& shader) {
    ComPtr<ID3D12RootSignature> rootSignature;

    ThrowIfFailed(_device->CreateRootSignature(
        0,
        shader.GetBufferPointer(),
        shader.GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    ));

    return rootSignature;
}

// Load Root Signature from the shader (must be defined in hlsl)
void LoadShaderRootSig(IDxcBlob& shader, ComPtr<ID3D12RootSignature>& rootSignature) {
    ThrowIfFailed(_device->CreateRootSignature(
        0,
        shader.GetBufferPointer(),
        shader.GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    ));
}

ComPtr<ID3D12RootSignature> LoadShaderRootSig(IDxcBlob& shader) {
    ComPtr<ID3D12RootSignature> rootSignature;

    ThrowIfFailed(_device->CreateRootSignature(
        0,
        shader.GetBufferPointer(),
        shader.GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    ));

    return rootSignature;
}

std::filesystem::path GetBinaryPath(const std::filesystem::path& file, const std::string& ext) {
    auto name = file.stem().string() + ext;
    return file.parent_path() / "bin" / name;
}

void CheckCompilerResult(IDxcResult* result) {
    ComPtr<IDxcBlobUtf8> pErrors;
    ThrowIfFailed(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(pErrors.GetAddressOf()), nullptr));
    if (pErrors && pErrors->GetStringLength() > 0) {
        throw std::exception((char*)pErrors->GetBufferPointer());
    }
}

void AddCommonArgs(std::vector<LPCWSTR>& args, LPCWSTR entryPoint, LPCWSTR profile) {
    args.push_back(L"-E"); // Entrypoint
    args.push_back(entryPoint);

    args.push_back(L"-T"); // Target profile
    args.push_back(profile);

    args.push_back(L"-I"); // Include directory
    args.push_back(L"shaders");
#ifdef _DEBUG
#else
    args.push_back(L"-no-warnings"); // warnings are grouped with errors and cause compilation to abort
#endif

    // -Fd pdb path to helper debuggers

    args.push_back(L"-Zi"); // Debug, profiling
    args.push_back(DXC_ARG_OPTIMIZATION_LEVEL0);
#ifdef _DEBUG
#else
    //args.push_back(L"-Qstrip_debug");
    //args.push_back(L"-Qstrip_reflect");
#endif
}

void LoadFile(const filesystem::path& file, ComPtr<ID3DBlob>& result) {
    uint32_t codePage = CP_UTF8;
    ComPtr<IDxcBlobEncoding> sourceBlob;
    ThrowIfFailed(_utils->LoadFile(file.c_str(), &codePage, &sourceBlob));
    ThrowIfFailed(sourceBlob.As(&result));
}

void CompileShader(const filesystem::path& file, span<LPCWSTR> args, ComPtr<ID3DBlob>& result) {
    uint32_t codePage = CP_UTF8;
    ComPtr<IDxcBlobEncoding> source;
    ThrowIfFailed(_utils->LoadFile(file.c_str(), &codePage, &source));

    DxcBuffer sourceBuffer{};
    sourceBuffer.Ptr = source->GetBufferPointer();
    sourceBuffer.Size = source->GetBufferSize();

    ComPtr<IDxcResult> dxcResult;
    ThrowIfFailed(_compiler->Compile(&sourceBuffer, args.data(), (uint32)args.size(), _includeHandler.Get(), IID_PPV_ARGS(&dxcResult)));
    CheckCompilerResult(dxcResult.Get());

    ComPtr<IDxcBlob> resultBlob;
    ThrowIfFailed(dxcResult->GetResult(&resultBlob));
    ThrowIfFailed(resultBlob.As(&result));
}

ComPtr<ID3DBlob> LoadComputeShader(const filesystem::path& file, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pso, string_view entryPoint) {
    ComPtr<ID3DBlob> shader;

    auto binaryPath = GetBinaryPath(file, ".bin");
    if (std::filesystem::exists(binaryPath)) {
        //SPDLOG_INFO("Loading compute shader {}", binaryPath.string());
        LoadFile(binaryPath, shader);
    }
    else {
        //if (!std::filesystem::exists("dxcompiler.dll") || !std::filesystem::exists("dxil.dll")) {
        //    auto message = fmt::format("Compiled shader {} not exist and dxcompiler.dll or dxil.dll is missing", binaryPath.string());
        //    throw Exception(message);
        //}

        SPDLOG_INFO("Compiling compute shader {}:{}", file.string(), entryPoint);
        List<LPCWSTR> args;
        auto wideEntry = Widen(entryPoint); // args takes a pointer to string, must keep it allocated
        AddCommonArgs(args, wideEntry.c_str(), COMPUTE_SHADER_VERSION);
        CompileShader(file, args, shader);
    }

    LoadShaderRootSig(*shader.Get(), rootSignature);

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.CS.pShaderBytecode = shader->GetBufferPointer();
    psoDesc.CS.BytecodeLength = shader->GetBufferSize();

    ThrowIfFailed(rootSignature->SetName(file.c_str()));
    ThrowIfFailed(_device->CreateComputePipelineState(
        &psoDesc,
        IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())
    ));
    ThrowIfFailed(pso->SetName(file.c_str()));

    return shader;
}

ComPtr<ID3DBlob> LoadVertexShader(const filesystem::path& file, string_view entryPoint) {
    ComPtr<ID3DBlob> shader;

    auto binaryPath = GetBinaryPath(file, ".vs.bin");
    if (filesystem::exists(binaryPath)) {
        //SPDLOG_INFO("Loading vertex shader {}", binaryPath.string());
        LoadFile(binaryPath, shader);
    }
    else {
        if (!filesystem::exists(file))
            throw Exception(fmt::format("Shader file not found:\n{}", file.string()));

        SPDLOG_INFO("Compiling vertex shader {}:{}", file.string(), entryPoint);

        List<LPCWSTR> args;
        auto wideEntry = Widen(entryPoint); // args takes a pointer to string, must keep it allocated
        AddCommonArgs(args, wideEntry.c_str(), VERTEX_SHADER_VERSION);
        CompileShader(file, args, shader);
    }

    return shader;
}

ComPtr<ID3DBlob> LoadPixelShader(const filesystem::path& file, string_view entryPoint) {
    ComPtr<ID3DBlob> shader;
    auto binaryPath = GetBinaryPath(file, ".ps.bin");
    if (filesystem::exists(binaryPath)) {
        //SPDLOG_INFO("Loading pixel shader {}", binaryPath.string());
        LoadFile(binaryPath, shader);
    }
    else {
        SPDLOG_INFO("Compiling pixel shader {}:{}", file.string(), entryPoint);

        List<LPCWSTR> args;
        auto wideEntry = Widen(entryPoint); // args takes a pointer to string, must keep it allocated
        AddCommonArgs(args, wideEntry.c_str(), PIXEL_SHADER_VERSION);
        CompileShader(file, args, shader);
    }

    return shader;
}

void InitShaderCompiler(ID3D12Device* device) {
    try {
        _device = device;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_utils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_compiler)));
        ThrowIfFailed(_utils->CreateDefaultIncludeHandler(&_includeHandler));
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error creating DXC compiler: {}", e.what());
    }
}

void FreeShaderCompiler() {
    ClearShaderCache();

    _device = nullptr;
    _utils.Reset();
    _compiler.Reset();
    _includeHandler.Reset();
}

CompiledShader CompileShader(const ShaderInfo& shader) {
    CompiledShader compiled;
    compiled.info = &shader;

    try {
        auto vertexShader = LoadVertexShader(shader.file, shader.vsEntryPoint);
        // load root sig from the shader hlsl
        compiled.rootSignature = LoadShaderRootSig(*vertexShader.Get());
        //ThrowIfFailed(compiled->rootSignature->SetName(shader.file.c_str()));

        auto pixelShader = LoadPixelShader(shader.file, shader.psEntryPoint);

        // Only assign shaders if they compiled successfully
        if (vertexShader && pixelShader) {
            compiled.vertexShader = vertexShader;
            compiled.pixelShader = pixelShader;
            SetName(compiled.rootSignature, shader.file);
        }

        return compiled;
    }
    catch (std::exception& e) {
        SPDLOG_ERROR(e.what());
        if (!compiled.vertexShader || !compiled.pixelShader) {
            auto msg = fmt::format("Unable to compile {}\n\n{}", shader.file, e.what());
            throw std::exception(msg.c_str()); // never initialized, crash
        }

        throw std::exception(fmt::format("error compiling shader {}", shader.file).c_str());
    }
}


// Orgb = srgb * Srgb + drgb * Drgb
constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_DESC_MULTIPLY_RT = {
    .BlendEnable = true,
    .LogicOpEnable = false,
    .SrcBlend = D3D12_BLEND_DEST_COLOR, // O = S * D
    .DestBlend = D3D12_BLEND_ZERO, // Zero out additive term
    .BlendOp = D3D12_BLEND_OP_ADD,
    .SrcBlendAlpha = D3D12_BLEND_ONE,
    .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
    .LogicOp = D3D12_LOGIC_OP_NOOP,
    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
};

constexpr D3D12_BLEND_DESC BLEND_DESC_MULTIPLY = {
    .RenderTarget = { BLEND_DESC_MULTIPLY_RT }
};

constexpr D3D12_DEPTH_STENCIL_DESC DEPTH_EQUAL =
{
    TRUE, // DepthEnable
    D3D12_DEPTH_WRITE_MASK_ZERO,
    D3D12_COMPARISON_FUNC_EQUAL, // DepthFunc
    FALSE, // StencilEnable
    D3D12_DEFAULT_STENCIL_READ_MASK,
    D3D12_DEFAULT_STENCIL_WRITE_MASK,
    {
        D3D12_STENCIL_OP_KEEP, // StencilFailOp
        D3D12_STENCIL_OP_KEEP, // StencilDepthFailOp
        D3D12_STENCIL_OP_KEEP, // StencilPassOp
        D3D12_COMPARISON_FUNC_ALWAYS // StencilFunc
    }, // FrontFace
    {
        D3D12_STENCIL_OP_KEEP, // StencilFailOp
        D3D12_STENCIL_OP_KEEP, // StencilDepthFailOp
        D3D12_STENCIL_OP_KEEP, // StencilPassOp
        D3D12_COMPARISON_FUNC_ALWAYS // StencilFunc
    } // BackFace
};


D3D12_GRAPHICS_PIPELINE_STATE_DESC BuildPipelineStateDesc(PipelineInfo& info, CompiledShader& shader, bool useStencil, uint msaaSamples, uint renderTargets) {
    using namespace DirectX;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    if (!shader.rootSignature || !shader.vertexShader || !shader.pixelShader)
        throw Exception("Shader is not valid");

    psoDesc.pRootSignature = shader.rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(shader.vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(shader.pixelShader.Get());
    psoDesc.InputLayout = shader.info->inputLayout;

    psoDesc.RasterizerState = [&info] {
        switch (info.culling) {
            case CullMode::None: return CommonStates::CullNone;
            case CullMode::Clockwise: return CommonStates::CullClockwise;
            case CullMode::CounterClockwise: default: return CommonStates::CullCounterClockwise;
            case CullMode::Wireframe: return CommonStates::Wireframe;
        }
    }();

    psoDesc.BlendState = [&info] {
        switch (info.blend) {
            case BlendMode::Alpha: return CommonStates::AlphaBlend;
            case BlendMode::StraightAlpha: return CommonStates::NonPremultiplied;
            case BlendMode::Additive: return CommonStates::Additive;
            case BlendMode::Opaque: default: return CommonStates::Opaque;
            case BlendMode::Multiply: return BLEND_DESC_MULTIPLY;
        }
    }();

    psoDesc.DSVFormat = useStencil ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_D32_FLOAT;

    psoDesc.DepthStencilState = [&info] {
        switch (info.depth) {
            case DepthMode::None: return CommonStates::DepthNone;
            case DepthMode::ReadWrite: return CommonStates::DepthDefault;
            case DepthMode::Read: default: return CommonStates::DepthRead;
            case DepthMode::ReadEqual: return DEPTH_EQUAL;
        }
    }();

    auto& stencil = psoDesc.DepthStencilState;

    if (info.stencil == StencilMode::PortalWrite) {
        stencil.StencilEnable = true;
        stencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        stencil.StencilReadMask = 0;
        stencil.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
        stencil.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        stencil.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
        stencil.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    }

    if (info.stencil == StencilMode::PortalRead) {
        stencil.StencilEnable = true;
        stencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        stencil.StencilWriteMask = 0;
        stencil.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        stencil.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        stencil.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
        stencil.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        stencil.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
        stencil.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    }

    if (info.stencil == StencilMode::PortalReadNeq) {
        stencil.StencilEnable = true;
        stencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        stencil.StencilWriteMask = 0;
        stencil.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        stencil.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        stencil.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        stencil.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        stencil.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        stencil.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    }

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = info.topology;
    psoDesc.NumRenderTargets = renderTargets;

    if (info.depth == DepthMode::ReadDecalBiased) {
        // Biases for decals
        psoDesc.RasterizerState.DepthBias = -10'000;
        psoDesc.RasterizerState.SlopeScaledDepthBias = -4.0f;
        psoDesc.RasterizerState.DepthBiasClamp = -100'000;
    }

    if (info.depth == DepthMode::ReadSpriteBiased) {
        // Biases for sprites
        psoDesc.RasterizerState.DepthBias = -20'000;
        psoDesc.RasterizerState.SlopeScaledDepthBias = -4.0f;
        psoDesc.RasterizerState.DepthBiasClamp = -200'000;
    }

    for (uint i = 0; i < renderTargets; i++)
        psoDesc.RTVFormats[i] = info.format;

    psoDesc.SampleDesc.Count = info.enableMultisample ? msaaSamples : 1;
    return psoDesc;
}

// shaders can be shared across multiple pipelines and should be cached
ID3D12RootSignature* GetRootSignature(const ShaderInfo* shader) {
    auto shaderAddr = (uintptr_t)shader;

    if (!_shaders.contains(shaderAddr))
        _shaders[shaderAddr] = CompileShader(*shader);

    return _shaders[shaderAddr].rootSignature.Get();
}

void CompileGraphicsPipeline(PipelineInfo& info) {
    if (!_compiler) throw std::exception("Shader compiler is not initializated. Was InitShaderCompiler() called?");
    ASSERT(info.shader);

    auto shaderAddr = (uintptr_t)info.shader;
    auto pipelineAddr = (uintptr_t)&info;

    auto rootSignature = GetRootSignature(info.shader);
    info.rootSignature = rootSignature;

    bool useStencil = false;
    uint msaaSamples = 1;
    auto desc = BuildPipelineStateDesc(info, _shaders[shaderAddr], useStencil, msaaSamples, 1);

    auto& pipeline = _pipelines[pipelineAddr];
    pipeline.rootSignature = rootSignature;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pso)));
    if (!info.name.empty()) std::ignore = pipeline.pso->SetName(Widen(info.name).c_str());

    pipeline.info = &info;
    info.pso = pipeline.pso.Get();
}

void ClearShaderCache() {
    for (auto& pipeline : _pipelines | views::values) {
        // clear state on the global pipeline info
        if (pipeline.info) {
            pipeline.info->pso = nullptr;
            pipeline.info->rootSignature = nullptr;
        }
    }

    _shaders.clear();
    _pipelines.clear();
}
}
