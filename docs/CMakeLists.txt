find_package(Doxygen REQUIRED) 
find_package(Sphinx REQUIRED)



# Поиск всех необходимых заголовков самого приложения
get_target_property(HEADER_DIRS hkreduce INCLUDE_DIRECTORIES)
file(GLOB_RECURSE HEADER_DIRS ${HEADER_DIRS}/*.h)


set(OUTPUT_DOCS_DIR ${CMAKE_CURRENT_BINARY_DIR})


# Указываем input для Doxygen
set(DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR}/src)
# Основная папка для вывода Doxygen
set(DOXYGEN_OUTPUT_DIR ${OUTPUT_DOCS_DIR}/doxygen)
# Куда положить сгенерированный "сайт" с документацией
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
# Шаблон конфиг файла Doxygen
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
# Путь до итогого форматированого конфиг файла Doxygen
set(DOXYFILE_OUT ${DOXYGEN_OUTPUT_DIR}/Doxyfile)



# Форматирование шаблона конфиг файла Doxygen
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY) 
# Создаем папку для вывода
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})
# Команда для генерации документации
add_custom_command(
    OUTPUT ${DOXYGEN_INDEX_FILE}
    DEPENDS ${HEADER_DIRS}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    WORKING_DIRECTORY ${DOXYGEN_OUTPUT_DIR}
    MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
    COMMENT "Generating docs"
    VERBATIM
)
# Добавляем "пустую" цель к процессу сборки с зависимостью в виде документации, которая генерируется командой выше.
# Это вызовет генерацию документации во время сборки
add_custom_target(doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})



set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/sphinx-source)
set(SPHINX_OUTPUT ${OUTPUT_DOCS_DIR}/sphinx)
set(SPHINX_INDEX_FILE ${SPHINX_OUTPUT}/index.html) 


# Создаем папку для вывода
file(MAKE_DIRECTORY ${SPHINX_OUTPUT})
# Sphinx будет запущен только если:
# - Doxygen перезапустился
# - Файлы документации были обновлены
# - Конфиг Sphinx был обновлен
add_custom_command(
    OUTPUT ${SPHINX_INDEX_FILE}
    COMMAND 
        ${SPHINX_EXECUTABLE} -b html
        # Tell Breathe where to find the Doxygen output
        -Dbreathe_projects.hkreduce=${DOXYGEN_OUTPUT_DIR}/xml
    ${SPHINX_SOURCE} ${SPHINX_OUTPUT}
    WORKING_DIRECTORY ${SPHINX_OUTPUT}
    DEPENDS ${SPHINX_SOURCE}/index.rst ${DOXYGEN_INDEX_FILE}
    MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
    COMMENT "Generating documentation with Sphinx"
)
# Также как и раньше добавим пустую цель к сборке
add_custom_target(sphinx ALL DEPENDS ${SPHINX_INDEX_FILE})


# # Add an install target to install the docs
# include(GNUInstallDirs)
# install(
#     DIRECTORY ${SPHINX_BUILD}
#     DESTINATION ${CMAKE_INSTALL_DOCDIR}
# )