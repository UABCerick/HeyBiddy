# Todas las apps de Hey Biddy se compilan en este componente.
# Cada carpeta de aqui (clock/, counter/, settings/...) es una app:
# se compilan TODOS sus .c (tambien en subcarpetas) y cada carpeta es ruta de include.
# Para agregar una app: crea su carpeta y agrega una linea en registry.c. No hace falta tocar este archivo.

file(GLOB_RECURSE APP_SRCS "${CMAKE_CURRENT_LIST_DIR}/*/*.c")
list(FILTER APP_SRCS EXCLUDE REGEX "/(host|test|tests)/")

file(GLOB APP_DIRS LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/*")
set(APP_INCLUDES ".")
foreach(dir ${APP_DIRS})
    if(IS_DIRECTORY ${dir})
        list(APPEND APP_INCLUDES ${dir})
    endif()
endforeach()

idf_component_register(SRCS "registry.c" ${APP_SRCS}
                       INCLUDE_DIRS ${APP_INCLUDES}
                       REQUIRES ui_core storage fonts lvgl esp_timer)

# Si cambia registry.c o aparece/desaparece un archivo en una carpeta de app,
# CMake vuelve a buscar los .c solo (asi un `git pull` con apps nuevas compila sin fullclean).
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(APP_WATCH "${CMAKE_CURRENT_LIST_DIR}/registry.c" "${CMAKE_CURRENT_LIST_DIR}")
    foreach(dir ${APP_DIRS})
        if(IS_DIRECTORY ${dir})
            list(APPEND APP_WATCH ${dir})
        endif()
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${APP_WATCH})
endif()
