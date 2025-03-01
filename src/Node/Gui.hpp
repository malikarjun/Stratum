#pragma once

#include <Core/PipelineState.hpp>
#include "NodeGraph.hpp"

#include <imgui.h>

namespace stm {

inline void imgui_vk_scalar(const char* label, vk::Format format, void* data) {
	switch (format) {
		case vk::Format::eR8Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U8, data, 1);
			break;
		case vk::Format::eR8G8Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U8, data, 2);
			break;
		case vk::Format::eR8G8B8Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U8, data, 3);
			break;
		case vk::Format::eR8G8B8A8Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U8, data, 4);
			break;

		case vk::Format::eR8Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S8, data, 1);
			break;
		case vk::Format::eR8G8Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S8, data, 2);
			break;
		case vk::Format::eR8G8B8Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S8, data, 3);
			break;
		case vk::Format::eR8G8B8A8Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S8, data, 4);
			break;
			
		case vk::Format::eR16Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U16, data, 1);
			break;
		case vk::Format::eR16G16Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U16, data, 2);
			break;
		case vk::Format::eR16G16B16Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U16, data, 3);
			break;
		case vk::Format::eR16G16B16A16Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U16, data, 4);
			break;

		case vk::Format::eR16Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S16, data, 1);
			break;
		case vk::Format::eR16G16Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S16, data, 2);
			break;
		case vk::Format::eR16G16B16Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S16, data, 3);
			break;
		case vk::Format::eR16G16B16A16Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S16, data, 4);
			break;

		case vk::Format::eR32Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U32, data, 1);
			break;
		case vk::Format::eR32G32Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U32, data, 2);
			break;
		case vk::Format::eR32G32B32Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U32, data, 3);
			break;
		case vk::Format::eR32G32B32A32Uint:
			ImGui::InputScalarN(label, ImGuiDataType_U32, data, 4);
			break;

		case vk::Format::eR32Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S32, data, 1);
			break;
		case vk::Format::eR32G32Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S32, data, 2);
			break;
		case vk::Format::eR32G32B32Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S32, data, 3);
			break;
		case vk::Format::eR32G32B32A32Sint:
			ImGui::InputScalarN(label, ImGuiDataType_S32, data, 4);
			break;
			
		case vk::Format::eR32Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Float, data, 1);
			break;
		case vk::Format::eR32G32Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Float, data, 2);
			break;
		case vk::Format::eR32G32B32Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Float, data, 3);
			break;
		case vk::Format::eR32G32B32A32Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Float, data, 4);
			break;
			
		case vk::Format::eR64Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Double, data, 1);
			break;
		case vk::Format::eR64G64Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Double, data, 2);
			break;
		case vk::Format::eR64G64B64Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Double, data, 3);
			break;
		case vk::Format::eR64G64B64A64Sfloat:
			ImGui::InputScalarN(label, ImGuiDataType_Double, data, 4);
			break;
	}
}

class Gui {
public:	
	STRATUM_API Gui(Node& node);
	STRATUM_API ~Gui();

	inline Node& node() const { return mNode; }
	
	STRATUM_API void create_pipelines();

	inline ImFont* font() const { return mFont; } 
	inline ImFont* title_font() const { return mTitleFont; } 
	
	STRATUM_API void create_font_image(CommandBuffer& commandBuffer);

	STRATUM_API void new_frame(CommandBuffer& commandBuffer, float deltaTime);
	STRATUM_API void make_geometry(CommandBuffer& commandBuffer);
	STRATUM_API void render(CommandBuffer& commandBuffer, const Image::View& dst);

	inline void set_context() const {
		ImGui::SetCurrentContext(mContext);
		ImGui::SetAllocatorFunctions(
			[](size_t sz, void* user_data) { return ::operator new(sz); },
			[](void* ptr, void* user_data) { ::operator delete(ptr); });
	}
	
	template<typename T>
	inline void register_inspector_gui_fn(void(*fn_ptr)(T*)) {
		mInspectorGuiFns[typeid(T)] = reinterpret_cast<void(*)(void*)>(fn_ptr);
	}
	inline void unregister_inspector_gui_fn(type_index t) {
		mInspectorGuiFns.erase(t);
	}

private:
	Node& mNode;
	component_ptr<GraphicsPipelineState> mPipeline;
	unordered_map<Image::View, uint32_t> mImageMap;
	Mesh mMesh;
	bool mUploadFonts = true;
	ImGuiContext* mContext;
	const ImDrawData* mDrawData;
	ImFont* mFont;
	ImFont* mTitleFont;

	unordered_map<type_index, void(*)(void*)> mInspectorGuiFns;

};

} 