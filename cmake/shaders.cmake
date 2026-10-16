# Compiles each shader with qsb, then embeds the results in one header, as meson.build does.
# Each array is named after its file: composite.vert -> composite_vert_qsb_data.
function(neoqcp_embed_shaders output)
    set(pairs)
    set(qsb_files)
    foreach(shader IN LISTS ARGN)
        get_filename_component(name ${shader} NAME)
        set(qsb ${CMAKE_CURRENT_BINARY_DIR}/shaders/${name}.qsb)
        add_custom_command(OUTPUT ${qsb}
            COMMAND Qt6::qsb --qt6 -o ${qsb} ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
            VERBATIM)
        string(REPLACE "." "_" var ${name})
        list(APPEND pairs "${var}_qsb_data:${qsb}")
        list(APPEND qsb_files ${qsb})
    endforeach()
    set(script ${CMAKE_CURRENT_SOURCE_DIR}/src/painting/shaders/embed_shaders.py)
    add_custom_command(OUTPUT ${output}
        COMMAND Python3::Interpreter ${script} ${output} ${pairs}
        DEPENDS ${script} ${qsb_files}
        VERBATIM)
endfunction()
