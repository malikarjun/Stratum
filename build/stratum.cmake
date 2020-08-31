set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(XR_USE_GRAPHICS_API_VULKAN)
if (${ENABLE_DEBUG_LAYERS})
	add_compile_definitions(ENABLE_DEBUG_LAYERS)
endif()

include_directories(
	"${STRATUM_HOME}"
	"${STRATUM_HOME}/ThirdParty/assimp/include"
	"${STRATUM_HOME}/ThirdParty/msdfgen/include"
	"${STRATUM_HOME}/ThirdParty/OpenXR-SDK/include")

if(WIN32)
	if(DEFINED ENV{VULKAN_SDK})
		message(STATUS "Found VULKAN_SDK: $ENV{VULKAN_SDK}")
	else()
		message(FATAL_ERROR "Error: VULKAN_SDK not set!")
	endif()

	add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
	add_compile_definitions(WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		add_compile_definitions(_CRTDBG_MAP_ALLOC)
	endif()

	link_directories("${STRATUM_HOME}/ThirdParty/msdfgen/freetype/win64")
	include_directories("$ENV{VULKAN_SDK}/include")
	link_libraries("Ws2_32.lib" "$ENV{VULKAN_SDK}/lib/vulkan-1.lib")

	if (${ENABLE_DEBUG_LAYERS})
		link_libraries("$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
	endif()
endif()

function(link_plugin TARGET_NAME)
	add_dependencies(${TARGET_NAME} Stratum)

	set_target_properties(${TARGET_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Plugins"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Plugins"
		ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib/Plugins"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/Plugins"
		LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/Plugins"
		ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib/Plugins"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Plugins"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Plugins"
		ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Plugins")

	if (WIN32)
		# Link vulkan and assimp
		target_link_libraries(${TARGET_NAME}
				"${PROJECT_BINARY_DIR}/lib/Stratum.lib"
				"$ENV{VULKAN_SDK}/lib/vulkan-1.lib" )
		if (CMAKE_BUILD_TYPE STREQUAL "Debug")
			target_link_libraries(${TARGET_NAME}
				"${STRATUM_HOME}/ThirdParty/assimp/lib/assimpd.lib"
				"${STRATUM_HOME}/ThirdParty/assimp/lib/zlibstaticd.lib"
				"${STRATUM_HOME}/ThirdParty/assimp/lib/IrrXMLd.lib" )
		else()
			target_link_libraries(${TARGET_NAME}
				"${STRATUM_HOME}/ThirdParty/assimp/lib/assimp.lib"
				"${STRATUM_HOME}/ThirdParty/assimp/lib/zlibstatic.lib"
				"${STRATUM_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )
		endif()

		if (${ENABLE_DEBUG_LAYERS})
			target_link_libraries(${TARGET_NAME} "$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
		endif()
	endif(WIN32)
endfunction()

function(add_shader_target TARGET_NAME FOLDER_PATH)
	# Compile shaders in Shaders/* using ShaderCompiler
	file(GLOB_RECURSE SHADER_SOURCES
		"${FOLDER_PATH}*.frag"
		"${FOLDER_PATH}*.vert"
		"${FOLDER_PATH}*.glsl"
		"${FOLDER_PATH}*.hlsl" )

	foreach(SHADER ${SHADER_SOURCES})
		get_filename_component(FILE_NAME ${SHADER} NAME_WE)
		set(SPIRV "${PROJECT_BINARY_DIR}/bin/Shaders/${FILE_NAME}.stmb")

		add_custom_command(
			OUTPUT ${SPIRV}
			COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/bin/Shaders/"
			COMMAND "${PROJECT_BINARY_DIR}/bin/ShaderCompiler" ${SHADER} ${SPIRV} "${STRATUM_HOME}/Shaders"
			DEPENDS ${SHADER})

		list(APPEND SPIRV_BINARY_FILES ${SPIRV})
	endforeach(SHADER)

	add_custom_target(${TARGET_NAME} DEPENDS ${SPIRV_BINARY_FILES})
	add_dependencies(${TARGET_NAME} ShaderCompiler)
endfunction()