#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

struct Variant
{
	std::string name;
	std::wstring source;
	std::string entry;
	std::string profile;
	std::vector<D3D_SHADER_MACRO> defines;
	bool expectRt6;
};

bool Compile(const Variant& variant, const fs::path& outDir, std::ofstream& report)
{
	ComPtr<ID3DBlob> bytecode;
	ComPtr<ID3DBlob> errors;
	const auto result = D3DCompileFromFile(
		variant.source.c_str(), variant.defines.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE,
		variant.entry.c_str(), variant.profile.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3,
		0, bytecode.GetAddressOf(), errors.GetAddressOf());
	if (errors && errors->GetBufferSize())
		std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()), static_cast<std::streamsize>(errors->GetBufferSize()));
	if (FAILED(result)) {
		report << variant.name << ": compile failed (0x" << std::hex << result << std::dec << ")\n";
		return false;
	}
	const auto dxbcPath = outDir / (variant.name + ".dxbc");
	std::ofstream dxbc(dxbcPath, std::ios::binary);
	dxbc.write(static_cast<const char*>(bytecode->GetBufferPointer()), static_cast<std::streamsize>(bytecode->GetBufferSize()));
	if (!dxbc)
		return false;
	ComPtr<ID3DBlob> disassembly;
	if (SUCCEEDED(D3DDisassemble(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), 0, nullptr, disassembly.GetAddressOf()))) {
		std::ofstream text(outDir / (variant.name + ".asm"), std::ios::binary);
		text.write(static_cast<const char*>(disassembly->GetBufferPointer()), static_cast<std::streamsize>(disassembly->GetBufferSize()));
	}

	ComPtr<ID3D11ShaderReflection> reflection;
	if (FAILED(D3DReflect(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), IID_PPV_ARGS(reflection.GetAddressOf()))))
		return false;
	D3D11_SHADER_DESC shaderDesc{};
	if (FAILED(reflection->GetDesc(&shaderDesc)))
		return false;

	bool hasRt6 = false;
	bool hasRt7 = false;
	bool hasT66 = false;
	report << "variant=" << variant.name << " profile=" << variant.profile << " dxbc_bytes=" << bytecode->GetBufferSize() << "\n";
	report << "  resources=" << shaderDesc.BoundResources << " outputs=" << shaderDesc.OutputParameters << "\n";
	for (UINT i = 0; i < shaderDesc.OutputParameters; ++i) {
		D3D11_SIGNATURE_PARAMETER_DESC output{};
		if (FAILED(reflection->GetOutputParameterDesc(i, &output)))
			return false;
		report << "  output=" << output.SemanticName << output.SemanticIndex << " register=" << output.Register << "\n";
		hasRt6 |= output.SemanticName && std::string_view(output.SemanticName) == "SV_Target" && output.SemanticIndex == 6;
		hasRt7 |= output.SemanticName && std::string_view(output.SemanticName) == "SV_Target" && output.SemanticIndex == 7;
	}
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
		D3D11_SHADER_INPUT_BIND_DESC binding{};
		if (FAILED(reflection->GetResourceBindingDesc(i, &binding)))
			return false;
		report << "  binding=" << binding.Name << " slot=" << binding.BindPoint << " count=" << binding.BindCount << " type=" << binding.Type << "\n";
		hasT66 |= binding.Name && std::string_view(binding.Name).ends_with("ParallaxShadowTexture") && binding.BindPoint == 66;
	}
	const bool isDirectionalLight = variant.name == "directional_light_shared_rt6";
	const bool valid = variant.expectRt6 == hasRt6 && (isDirectionalLight || hasRt7) && (!isDirectionalLight || hasT66);
	report << "  contract=" << (valid ? "PASS" : "FAIL") << "\n";
	if (!valid)
		return false;
	return true;
}

std::vector<D3D_SHADER_MACRO> Defines(std::initializer_list<D3D_SHADER_MACRO> list)
{
	std::vector<D3D_SHADER_MACRO> result(list);
	result.push_back({ nullptr, nullptr });
	return result;
}

int wmain(int argc, wchar_t** argv)
{
	if (argc != 3) {
		std::wcerr << L"usage: compile_transport_shaders <repo-root> <output-dir>\n";
		return 2;
	}
	const fs::path root = argv[1];
	const fs::path outDir = argv[2];
	fs::create_directories(outDir);
	std::ofstream report(outDir / "shader-reflection.txt", std::ios::trunc);
	const auto prepass = (root / L"build/ShaderStage/Shaders/DFPrepass.hlsl").wstring();
	const auto base = Defines({ { "BSDFPREPASS_PS_SOURCE", "1" }, { "FRAMEBUFFER_REGISTER", "b4" }, { "FACE", "1" },
		{ "SUBSURFACE_SCATTERING", "1" } });
	std::vector<Variant> variants;
	variants.push_back({ "parallax_off_face_diag_off", prepass, "main", "ps_5_0", base, false });
	auto parallax = base;
	parallax.insert(parallax.end() - 1, { "EXTENDED_MATERIALS", "1" });
	variants.push_back({ "parallax_on_face_diag_off", prepass, "main", "ps_5_0", parallax, true });
	auto faceOnly = base;
	faceOnly.insert(faceOnly.end() - 1, { "FACE_TRANSPORT_DIAGNOSTIC", "1" });
	faceOnly.insert(faceOnly.end() - 1, { "FACE_TRANSPORT_RGBA8", "1" });
	variants.push_back({ "parallax_off_face_diag_on", prepass, "main", "ps_5_0", faceOnly, true });
	auto both = faceOnly;
	both.insert(both.end() - 1, { "EXTENDED_MATERIALS", "1" });
	variants.push_back({ "parallax_on_face_diag_on", prepass, "main", "ps_5_0", both, true });

	const auto light = (root / L"build/ShaderStage/Shaders/DFLight.hlsl").wstring();
	auto lightDefines = Defines({ { "BSDFLIGHT_PS_DIRSPLITS2", "1" }, { "DIRECTIONAL", "1" }, { "LIGHT_TYPE", "1" },
		{ "DIRSPLITS", "2" }, { "SHADOW", "1" }, { "RGBSPEC", "1" }, { "SPECULAR", "1" },
		{ "FRAMEBUFFER_REGISTER", "b4" }, { "FO4CS_SUBSTRATE", "1" }, { "EXTENDED_MATERIALS", "1" },
		{ "FACE_TRANSPORT_DIAGNOSTIC", "1" }, { "FACE_TRANSPORT_RGBA8", "1" } });
	variants.push_back({ "directional_light_shared_rt6", light, "main", "ps_5_0", lightDefines, false });

	bool success = true;
	for (const auto& variant : variants)
		success &= Compile(variant, outDir, report);
	return success ? 0 : 1;
}
